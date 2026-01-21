#pragma once
#include "Base.hpp"

#include "volcano/net/Connection.hpp"
#include "volcano/mud/ClientData.hpp"

namespace volcano::telnet {

    class TelnetConnection {
        friend class TelnetOption;

        public:
        TelnetConnection(volcano::net::AnyStream connection);
        
        boost::asio::awaitable<void> run();
        boost::asio::awaitable<void> negotiateOptions(boost::asio::steady_timer::duration negotiation_timeout);
        void requestAbort();

        const volcano::mud::ClientData& client_data() const {
            return client_data_;
        }

        Channel<TelnetToGameMessage>& to_game_channel() {
            return to_game_messages_;
        }

        boost::asio::awaitable<void> sendToClient(const TelnetFromGameMessage& msg);

        private:
        volcano::net::AnyStream conn_;
        volcano::mud::ClientData client_data_;
        std::vector<std::shared_ptr<Channel<bool>>> pending_channels_;
        Channel<TelnetMessage> outgoing_messages_;
        Channel<TelnetToGameMessage> to_game_messages_;
        std::atomic_bool abort_requested_{false};
        std::string append_data_buffer_;
        
        boost::asio::awaitable<void> runReader();
        boost::asio::awaitable<void> runWriter();
        
        boost::asio::awaitable<void> processData(TelnetMessage& data);

        boost::asio::awaitable<void> sendAppData(std::string_view app_data);
        boost::asio::awaitable<void> sendSubNegotiation(char option, std::string_view sub_data);
        boost::asio::awaitable<void> sendNegotiation(char command, char option);

        boost::asio::awaitable<void> notifyChangedCapabilities(nlohmann::json& capabilities);

        std::unordered_map<char, std::shared_ptr<TelnetOption>> options_;

        // event handlers
        boost::asio::awaitable<void> processAppData(TelnetMessageData& app_data);
    };

    inline auto format_as(const TelnetConnection& telnet_connection) {
        auto &cd = telnet_connection.client_data();
        return fmt::format("TelnetConnection{{id={}, address={}, hostname={}, tls={}}}",
            cd.connection_id,
            cd.client_address,
            cd.client_hostname,
            cd.tls ? "true" : "false"
        );
    }

} // namespace vol::telnet