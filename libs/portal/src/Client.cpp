#include "volcano/net/net.hpp"
#include "volcano/log/Log.hpp"
#include "volcano/portal/Client.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/redirect_error.hpp>
#include "volcano/mud/ClientDataSave.hpp"

#include <chrono>

namespace volcano::portal {

    volcano::web::HttpTarget target;
    std::function<std::shared_ptr<ModeHandler>(Client& client)> create_initial_mode_handler;
    std::function<boost::asio::awaitable<std::optional<JwtTokens>>(Client& client)> handle_refresh_timer;

    ModeHandler::ModeHandler(Client& client)
    : client_(client)
    {
        
    }

    boost::asio::awaitable<void> ModeHandler::enterMode() {
        co_return;
    }

    boost::asio::awaitable<void> ModeHandler::exitMode() {
        co_return;
    }

    boost::asio::awaitable<void> ModeHandler::runImpl() {
        co_await volcano::net::waitForever(cancellation_signal_);
        co_return;
    }

    boost::asio::awaitable<void> ModeHandler::run() {
        using namespace boost::asio::experimental::awaitable_operators;
        co_await enterMode();
        co_await (runTelnetReader() || runImpl());
        co_await exitMode();
        co_return;
    }

    void ModeHandler::requestCancel() {
        cancellation_signal_.emit(boost::asio::cancellation_type::all);
    }

    boost::asio::cancellation_slot ModeHandler::cancellationSlot() {
        return cancellation_signal_.slot();
    }

    boost::asio::awaitable<void> ModeHandler::requestMode(std::shared_ptr<ModeHandler> next, bool cancel_self) {
        co_await client_.enqueueMode(std::move(next));
        if(cancel_self) {
            requestCancel();
        }
        co_return;
    }

    boost::asio::awaitable<void> ModeHandler::runTelnetReader() {
        auto &chan = client_.telnetToGameChannel();

        for(;;) {
            boost::system::error_code ec;
            auto msg = co_await chan.async_receive(
                boost::asio::bind_cancellation_slot(
                    cancellation_signal_.slot(),
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec))
            );

            if(ec) {
                if(ec == boost::asio::error::operation_aborted) {
                    co_return;
                }
                co_return;
            }

            if(std::holds_alternative<volcano::telnet::TelnetDisconnect>(msg)) {
                co_await handleDisconnect();
                co_return;
            }

            auto &game_msg = std::get<volcano::telnet::TelnetGameMessage>(msg);
            if(std::holds_alternative<volcano::telnet::TelnetMessageData>(game_msg)) {
                const auto &data_msg = std::get<volcano::telnet::TelnetMessageData>(game_msg);
                co_await handleCommand(data_msg.data);
            } else if(std::holds_alternative<volcano::telnet::TelnetMessageGMCP>(game_msg)) {
                const auto &gmcp_msg = std::get<volcano::telnet::TelnetMessageGMCP>(game_msg);
                co_await handleGMCP(gmcp_msg.package, gmcp_msg.data);
            } else if(std::holds_alternative<volcano::telnet::TelnetChangeCapabilities>(game_msg)) {
                const auto &cap_msg = std::get<volcano::telnet::TelnetChangeCapabilities>(game_msg);
                co_await client_.changeCapabilities(cap_msg.capabilities);
            }
        }
    }

    boost::asio::awaitable<void> ModeHandler::handleCommand(const std::string& data) {
        co_return;
    }

    boost::asio::awaitable<void> ModeHandler::handleGMCP(const std::string& package, const nlohmann::json& data) {
        co_return;
    }

    boost::asio::awaitable<void> ModeHandler::handleDisconnect() {
        requestCancel();
        co_return;
    }

    Client::Client(std::shared_ptr<volcano::telnet::TelnetLink> link)
    : link_(std::move(link)), mode_handler_channel_(volcano::net::context(), 2), http_client_(target)
    {
        if (link_) {
            client_info_.address = link_->address;
            client_info_.hostname = link_->hostname;
        }
    }

    volcano::web::HttpRequest Client::createBaseRequest(boost::beast::http::verb method, const std::string& target_path)
    {
        volcano::web::HttpRequest req{method, target_path, 11};
        req.set(boost::beast::http::field::host, client_info_.hostname);
        if(tokens) {
            req.set(boost::beast::http::field::authorization, "Bearer " + tokens->jwt);
        }
        req.set(boost::beast::http::field::user_agent, "volcano-portal/1.0");
        req.set(boost::beast::http::field::x_forwarded_for, client_info_.address.to_string());
        return req;
    }

    volcano::web::HttpRequest Client::createAuthenticatedRequest(boost::beast::http::verb method, const std::string& target_path)
    {
        auto req = createBaseRequest(method, target_path);
        req.prepare_payload();
        return req;
    }

    volcano::web::HttpRequest Client::createJsonRequest(boost::beast::http::verb method, const std::string& target_path, const nlohmann::json& j)
    {
        auto req = createBaseRequest(method, target_path);
        req.set(boost::beast::http::field::content_type, "application/json");
        req.body() = j.dump();
        req.prepare_payload();
        return req;
    }

    boost::asio::awaitable<void> Client::runMode()
    {
        // in a loop, retrieve mode handlers from the channel and co_await their .run().
        for(;;) {
            boost::system::error_code ec;
            auto next = co_await mode_handler_channel_.async_receive(
                boost::asio::redirect_error(boost::asio::use_awaitable, ec)
            );

            if(ec) {
                if(ec == boost::asio::error::operation_aborted) {
                    co_return;
                }
                co_return;
            }

            if(!next) {
                continue;
            }

            mode_handler_ = std::move(next);
            co_await mode_handler_->run();
            mode_handler_.reset();
        }

        co_return;
    }

    boost::asio::awaitable<void> Client::run() {
        using namespace boost::asio::experimental::awaitable_operators;

        //  first swap to the initial mode handler...
        auto handler = create_initial_mode_handler(*this);
        co_await enqueueMode(std::move(handler));

        // then run all the tasks.
        co_await (runMode() || runRefresher());

        co_return;
    }

    boost::asio::awaitable<void> Client::runRefresher()
    {
        if (!handle_refresh_timer) {
            LERROR("No refresh handler configured for portal client refresher.");
            co_return;
        }

        boost::asio::steady_timer timer(volcano::net::context());

        for (;;) {
            if (!tokens) {
                timer.expires_after(std::chrono::seconds(1));
                boost::system::error_code ec;
                co_await timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    if (ec == boost::asio::error::operation_aborted) {
                        co_return;
                    }
                }
                continue;
            }

            auto wait_for = tokens->expires_in;
            if (wait_for <= boost::asio::steady_timer::duration::zero()) {
                co_await handle_refresh_timer(*this);
                continue;
            }

            timer.expires_after(wait_for);
            boost::system::error_code ec;
            co_await timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                if (ec == boost::asio::error::operation_aborted) {
                    co_return;
                }
                continue;
            }

            if (!handle_refresh_timer) {
                LERROR("No refresh handler configured for portal client refresher.");
                co_return;
            }

            auto res = co_await handle_refresh_timer(*this);
            if(res) {
                tokens = std::move(*res);
            } else {
                tokens.reset();
                // we should probably notify the mode handler here...
                co_return;
            }
        }
    }

    boost::asio::awaitable<void> Client::sendText(const std::string& text)
    {
        if (!link_ || !link_->to_telnet) {
            co_return;
        }
        boost::system::error_code ec;
        co_await link_->to_telnet->async_send(
            ec,
            volcano::telnet::TelnetMessageData{text},
            boost::asio::use_awaitable);
    }

    boost::asio::awaitable<void> Client::sendLine(const std::string& text)
    {
        if (!link_ || !link_->to_telnet) {
            co_return;
        }
        if(boost::algorithm::ends_with(text, "\r\n")) {
            co_await sendText(text);
        } else {
            co_await sendText(text + "\r\n");
        }
        co_return;
    }

    boost::asio::awaitable<void> Client::sendGMCP(const std::string& package, const nlohmann::json& data)
    {
        if (!link_ || !link_->to_telnet) {
            co_return;
        }
        boost::system::error_code ec;
        co_await link_->to_telnet->async_send(
            ec,
            volcano::telnet::TelnetMessageGMCP{package, data},
            boost::asio::use_awaitable);
    }

    boost::asio::awaitable<void> Client::enqueueMode(std::shared_ptr<ModeHandler> next)
    {
        boost::system::error_code ec;
        co_await mode_handler_channel_.async_send(ec, std::move(next), boost::asio::use_awaitable);
        co_return;
    }

    volcano::telnet::Channel<volcano::telnet::TelnetToGameMessage>& Client::telnetToGameChannel()
    {
        return *link_->to_game;
    }

    boost::asio::awaitable<void> Client::changeCapabilities(const nlohmann::json& j)
    {
        j.get_to(link_->client_data);
        co_return;
    }

    boost::asio::awaitable<void> handle_telnet(volcano::net::AnyStream&& stream)
    {
        volcano::telnet::TelnetConnection telnet(std::move(stream));
        LINFO("Starting telnet connection handler for {}", telnet);
        co_await telnet.run();
        LTRACE("Telnet connection handler for {} has exited.", telnet);
        co_return;
    }

    boost::asio::awaitable<void> run_portal_links()
    {
        LINFO("Starting portal link handler.");
        auto& channel = volcano::telnet::link_channel();
        for(;;) {
            boost::system::error_code ec;
            auto link = co_await channel.async_receive(
                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if(ec) {
                if(ec == boost::asio::error::operation_aborted) {
                    co_return;
                }
                continue;
            }

            if(!link) {
                LERROR("Received null link in portal link handler.");
                continue;
            }

            auto strand = boost::asio::make_strand(volcano::net::context());
            boost::asio::co_spawn(
                strand,
                [link]() -> boost::asio::awaitable<void> {
                    LINFO("Starting portal client handler for {}", *link);
                    Client client(link);
                    co_await client.run();
                    LTRACE("Portal client handler for {} has exited.", *link);
                    co_return;
                },
                boost::asio::detached);
        }
    }
}