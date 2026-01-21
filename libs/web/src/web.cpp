#include <volcano/web/web.hpp>

#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/url.hpp>

namespace volcano::web {

static HttpResponse make_response(const HttpRequest& req, HttpAnswer answer) {
	HttpResponse res{answer.status, req.version()};
	res.set(answer.content_type_field, answer.content_type);
	res.keep_alive(req.keep_alive());
	res.body() = std::move(answer.body);
	res.prepare_payload();
	return res;
}

volcano::net::ClientHandler make_router_handler(std::shared_ptr<Router> router) {
	return [router](volcano::net::AnyStream&& stream) -> boost::asio::awaitable<void> {
		boost::beast::flat_buffer buffer;

		for (;;) {
			HttpRequest req;
			boost::system::error_code ec;
			co_await http::async_read(stream, buffer, req, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
			if (ec == http::error::end_of_stream) {
				break;
			}
			if (ec) {
				co_return;
			}

			auto parsed = boost::urls::parse_origin_form(req.target());
			std::string path_storage;
			if (parsed) {
				path_storage = std::string(parsed->path());
			} else {
				auto target = req.target();
				path_storage = std::string(target);
			}
			std::string_view path = path_storage;

			auto match = router->match(path);
			if (!match) {
				HttpAnswer answer{http::status::not_found, "Not Found"};
				auto res = make_response(req, std::move(answer));
				co_await http::async_write(stream, res, boost::asio::use_awaitable);
				continue;
			}

			auto& node = *match->node;
			auto params = std::move(match->params);

			if (boost::beast::websocket::is_upgrade(req)) {
				auto ws_handler = node.websocket_handler();
				if (!ws_handler) {
					HttpAnswer answer{http::status::not_found, "Not Found"};
					auto res = make_response(req, std::move(answer));
					co_await http::async_write(stream, res, boost::asio::use_awaitable);
					continue;
				}

				WebSocketStream ws(std::move(stream));
				co_await ws.async_accept(req, boost::asio::use_awaitable);
				co_await ws_handler->get()(ws, req, params);
				co_return;
			}

			auto handler = node.request_handler(req.method());
			if (!handler) {
				HttpAnswer answer{node.has_request_handlers() ? http::status::method_not_allowed : http::status::not_found,
								  node.has_request_handlers() ? "Method Not Allowed" : "Not Found"};
				auto res = make_response(req, std::move(answer));
				co_await http::async_write(stream, res, boost::asio::use_awaitable);
				continue;
			}

			auto answer = co_await handler->get()(stream, req, params);
			auto res = make_response(req, std::move(answer));
			co_await http::async_write(stream, res, boost::asio::use_awaitable);
		}

		boost::system::error_code ec;
		stream.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
		co_return;
	};
}

} // namespace vol::web
