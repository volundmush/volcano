#pragma once
#include "Base.hpp"

#include "volcano/net/Connection.hpp"
#include "volcano/mud/ClientData.hpp"

#include <memory>

namespace volcano::telnet {

    class TelnetConnection {
        friend class TelnetOption;

        public:
        TelnetConnection(volcano::net::AnyStream connection);
        
        boost::asio::awaitable<TelnetDisconnect> run();
        boost::asio::awaitable<void> negotiateOptions();
        
        boost::asio::awaitable<void> setClientName(const std::string& name, const std::string& version);

        const volcano::mud::ClientData& client_data() const {
            return client_data_;
        }

        Channel<TelnetToGameMessage>& to_game_channel() {
            return *to_game_messages_;
        }

        std::shared_ptr<Channel<TelnetToGameMessage>> to_game_channel_shared() const {
            return to_game_messages_;
        }

        std::shared_ptr<Channel<TelnetToTelnetMessage>> to_telnet_channel_shared() const {
            return to_telnet_messages_;
        }

        bool is_negotiation_completed() const {
            return negotiation_completed_;
        }

        std::shared_ptr<TelnetLink> make_link() const;

        boost::asio::awaitable<void> sendToClient(const TelnetToTelnetMessage& msg);

        const volcano::net::AnyStream& connection() const {
            return conn_;
        }

        private:
        volcano::net::AnyStream conn_;
        boost::asio::steady_timer keepalive_timer_;
        volcano::mud::ClientData client_data_;
        std::vector<std::shared_ptr<Channel<bool>>> pending_channels_;
        Channel<TelnetOutgoingMessage> outgoing_messages_;
        std::shared_ptr<Channel<TelnetToTelnetMessage>> to_telnet_messages_;
        std::shared_ptr<Channel<TelnetToGameMessage>> to_game_messages_;
        std::atomic<TelnetDisconnect> shutdown_reason_{TelnetDisconnect::error};
        std::string append_data_buffer_;
        bool telnet_mode{false};
        boost::asio::cancellation_signal cancellation_signal_;
        boost::asio::cancellation_state cancellation_state_;
        bool negotiation_completed_{false};

        boost::asio::awaitable<void> runReader();
        boost::asio::awaitable<void> runWriter();
        boost::asio::awaitable<void> runLink();
        boost::asio::awaitable<void> runOutboundBridge();
        boost::asio::awaitable<void> runKeepAlive();

        boost::asio::awaitable<void> signalShutdown(TelnetDisconnect reason);
        
        boost::asio::awaitable<void> processData(TelnetMessage& data);

        boost::asio::awaitable<void> sendAppData(std::string_view app_data);
        boost::asio::awaitable<void> sendSubNegotiation(char option, std::string_view sub_data);
        boost::asio::awaitable<void> sendNegotiation(char command, char option);
        boost::asio::awaitable<void> sendCommand(char command);
        boost::asio::awaitable<void> notifyChangedCapabilities(nlohmann::json& capabilities);

        std::unordered_map<char, std::shared_ptr<TelnetOption>> options_;

        // event handlers
        boost::asio::awaitable<void> handleAppData(TelnetMessageData& app_data);
        boost::asio::awaitable<void> handleGMCP(TelnetMessageGMCP& gmcp);
        boost::asio::awaitable<void> handleNegotiate(TelnetMessageNegotiation& negotiation);
        boost::asio::awaitable<void> handleSubNegotiation(TelnetMessageSubnegotiation& subnegotiation);
        boost::asio::awaitable<void> handleCommand(TelnetMessageCommand& command);
    };

    Channel<std::shared_ptr<TelnetLink>>& link_channel();

    inline auto format_as(const TelnetConnection& telnet_connection) {
        auto &cd = telnet_connection.client_data();
        return fmt::format("TelnetConnection({})",
            telnet_connection.connection()
        );
    }

} // namespace volcano::telnet