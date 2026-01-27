#pragma once
#include "volcano/net/Connection.hpp"
#include "volcano/telnet/Connection.hpp"
#include "volcano/web/web.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/steady_timer.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>


namespace volcano::portal {
    template<typename T>
    using Channel = boost::asio::experimental::concurrent_channel<void(boost::system::error_code, T)>;

    extern volcano::web::HttpTarget target;

    class Client;

    class ModeHandler {
        public:
        ModeHandler(Client& client);
        virtual ~ModeHandler() = default;

        boost::asio::awaitable<void> run();
        void requestCancel();

        protected:

        boost::asio::cancellation_slot cancellationSlot();
        boost::asio::awaitable<void> requestMode(std::shared_ptr<ModeHandler> next, bool cancel_self = true);

        boost::asio::awaitable<void> runTelnetReader();
        virtual boost::asio::awaitable<void> handleCommand(const std::string& data);
        virtual boost::asio::awaitable<void> handleGMCP(const std::string& package, const nlohmann::json& data);
        virtual boost::asio::awaitable<void> handleDisconnect();

        virtual boost::asio::awaitable<void> enterMode();
        virtual boost::asio::awaitable<void> exitMode();

        virtual boost::asio::awaitable<void> runImpl();

        Client& client_;
        boost::asio::cancellation_signal cancellation_signal_;
    };

    struct JwtTokens {
        std::string jwt;
        std::string refresh;
        boost::asio::steady_timer::duration expires_in;
    };

    class Client {
        public:
        explicit Client(std::shared_ptr<volcano::telnet::TelnetLink> link);

        boost::asio::awaitable<void> run();

        boost::asio::awaitable<void> sendText(const std::string& text);
        boost::asio::awaitable<void> sendLine(const std::string& text);
        boost::asio::awaitable<void> sendGMCP(const std::string& package, const nlohmann::json& data);

        boost::asio::awaitable<void> enqueueMode(std::shared_ptr<ModeHandler> next);
        volcano::telnet::Channel<volcano::telnet::TelnetToGameMessage>& telnetToGameChannel();

        Channel<std::shared_ptr<ModeHandler>> mode_handler_channel_;
        boost::asio::awaitable<void> changeCapabilities(const nlohmann::json& j);

        volcano::web::HttpClient& httpClient() {
            return http_client_;
        }

        volcano::web::HttpRequest createBaseRequest(boost::beast::http::verb method, const std::string& target_path);
        volcano::web::HttpRequest createAuthenticatedRequest(boost::beast::http::verb method, const std::string& target_path);
        volcano::web::HttpRequest createJsonRequest(boost::beast::http::verb method, const std::string& target_path, const nlohmann::json& j);

        std::optional<JwtTokens> tokens;

        volcano::mud::ClientData& clientData() {
            return link_->client_data;
        }

        private:
        std::shared_ptr<volcano::telnet::TelnetLink> link_;
        volcano::web::ClientInfo client_info_;
        volcano::web::HttpClient http_client_;
        boost::asio::awaitable<void> runMode();
        boost::asio::awaitable<void> runRefresher();

        // the base64 sequence after Authentication Bearer.
        
        std::shared_ptr<ModeHandler> mode_handler_;
    };

    extern std::function<std::shared_ptr<ModeHandler>(Client& client)> create_initial_mode_handler;
    extern std::function<boost::asio::awaitable<std::optional<JwtTokens>>(Client& client)> handle_refresh_timer;

    boost::asio::awaitable<void> handle_telnet(volcano::net::AnyStream&& stream);
    boost::asio::awaitable<void> run_portal_links();
}