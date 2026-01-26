#include <volcano/web/web.hpp>

#include <optional>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include "volcano/net/Connection.hpp"
#include "volcano/net/net.hpp"


namespace volcano::web {

static std::optional<boost::asio::ip::address> parse_forwarded_address(std::string_view raw) {
	std::string trimmed = boost::algorithm::trim_copy(std::string(raw));
	if (trimmed.empty()) {
		return std::nullopt;
	}

	if (trimmed.front() == '[') {
		auto end = trimmed.find(']');
		if (end != std::string::npos) {
			trimmed = trimmed.substr(1, end - 1);
		}
	}

	const auto first_colon = trimmed.find(':');
	if (first_colon != std::string::npos && trimmed.find(':', first_colon + 1) == std::string::npos) {
		trimmed = trimmed.substr(0, first_colon);
	}

	auto parsed = volcano::net::parse_address(trimmed);
	if (!parsed) {
		return std::nullopt;
	}
	return *parsed;
}

static std::vector<boost::asio::ip::address> parse_x_forwarded_for(std::string_view value) {
	std::vector<boost::asio::ip::address> addresses;
	std::size_t start = 0;
	while (start < value.size()) {
		auto end = value.find(',', start);
		if (end == std::string_view::npos) {
			end = value.size();
		}
		auto token = value.substr(start, end - start);
		if (auto parsed = parse_forwarded_address(token)) {
			addresses.push_back(*parsed);
		}
		start = end + 1;
	}
	return addresses;
}

static HttpResponse make_response(const HttpRequest& req, HttpAnswer answer) {
	HttpResponse res{answer.status, req.version()};
	res.set(http::field::content_type, answer.content_type);
	res.keep_alive(req.keep_alive());
	res.body() = std::move(answer.body);
	res.prepare_payload();
	return res;
}

std::expected<nlohmann::json, std::string> parse_json_body(HttpRequest& req) {
	try {
		auto json = nlohmann::json::parse(req.body());
		return json;
	} catch (const nlohmann::json::parse_error& e) {
		return std::unexpected(std::string("Failed to parse JSON body: ") + e.what());
	}
}

std::expected<nlohmann::json, HttpAnswer> authorize_bearer(HttpRequest& req, const volcano::jwt::JwtContext& jwt_ctx) {
	auto auth_it = req.find(http::field::authorization);
	if (auth_it == req.end()) {
		return std::unexpected(HttpAnswer{http::status::unauthorized, "Authorization header missing"});
	}

	std::string_view auth_value = auth_it->value();
	const std::string_view bearer_prefix = "Bearer ";
	if (!boost::algorithm::istarts_with(auth_value, bearer_prefix)) {
		return std::unexpected(HttpAnswer{http::status::unauthorized, "Invalid authorization scheme"});
	}

	std::string_view token = auth_value.substr(bearer_prefix.size());
	auto verify_result = jwt_ctx.verify(token);
	if (!verify_result) {
		return std::unexpected(HttpAnswer{http::status::unauthorized, "Token verification failed: " + verify_result.error()});
	}

	return *verify_result;
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

			ClientInfo connection_info{
				.hostname = stream.hostname(),
				.address = stream.endpoint().address(),
			};
			ClientInfo client_info = connection_info;

			auto& node = *match->node;
			auto params = std::move(match->params);

			if (router->is_trusted_proxy(connection_info.address)) {
				std::optional<boost::asio::ip::address> forwarded_address;

				auto forwarded_for = req.find("X-Forwarded-For");
				if (forwarded_for != req.end()) {
					auto list = parse_x_forwarded_for(forwarded_for->value());
					if (!list.empty()) {
						for (auto it = list.rbegin(); it != list.rend(); ++it) {
							if (!router->is_trusted_proxy(*it)) {
								forwarded_address = *it;
								break;
							}
						}
						if (!forwarded_address) {
							forwarded_address = list.front();
						}
					}
				}

				if (!forwarded_address) {
					auto origin_ip = req.find("X-Origin-Ip");
					if (origin_ip != req.end()) {
						forwarded_address = parse_forwarded_address(origin_ip->value());
					}
				}

				if (forwarded_address) {
					client_info.address = *forwarded_address;
					client_info.hostname = client_info.address.to_string();
				}
			}

			auto ctx = RequestContext{client_info, connection_info, req, params, {}, nlohmann::json::object()};
			ctx.query = boost::urls::url_view(req.target()).params();

			if (boost::beast::websocket::is_upgrade(req)) {
				auto ws_handler = node.websocket_handler();
				if (!ws_handler) {
					HttpAnswer answer{http::status::not_found, "Not Found"};
					auto res = make_response(req, std::move(answer));
					co_await http::async_write(stream, res, boost::asio::use_awaitable);
					continue;
				}

				auto &ws_endpoint = ws_handler->get();
				if (ws_endpoint.guard) {
					if (auto guard_answer = co_await ws_endpoint.guard(stream, ctx); guard_answer) {
						auto res = make_response(req, std::move(*guard_answer));
						co_await http::async_write(stream, res, boost::asio::use_awaitable);
						continue;
					}
				}

				WebSocketStream ws(std::move(stream));
				co_await ws.async_accept(req, boost::asio::use_awaitable);
				co_await ws_endpoint.handler(ws, ctx);
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

			auto &endpoint = handler->get();
			if (endpoint.guard) {
				if (auto guard_answer = co_await endpoint.guard(stream, ctx); guard_answer) {
					auto res = make_response(req, std::move(*guard_answer));
					co_await http::async_write(stream, res, boost::asio::use_awaitable);
					continue;
				}
			}

			auto answer = co_await endpoint.handler(stream, ctx);
			auto res = make_response(req, std::move(answer));
			co_await http::async_write(stream, res, boost::asio::use_awaitable);
		}

		boost::system::error_code ec;
		stream.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
		co_return;
	};
}

} // namespace vol::web
