#pragma once

#include "telnet/Base.hpp"

namespace vol::telnet {
    struct TelnetOptionState {
        bool enabled{false};
        bool negotiating{false};
    };

    class TelnetOption {
        public:
        TelnetOption(TelnetConnection& connection);
        virtual ~TelnetOption() = default;

        Channel<TelnetToGameMessage> to_game;
        Channel<TelnetFromGameMessage> from_game;

        virtual char option_code() const = 0;
        
        virtual boost::asio::awaitable<void> start();
        virtual boost::asio::awaitable<void> send_negotiation(char command);
        virtual boost::asio::awaitable<void> send_subnegotiate(std::string_view data);

        virtual boost::asio::awaitable<void> at_send_negotiate(char command);
        virtual boost::asio::awaitable<void> at_send_subnegotiate(std::string_view data);

        virtual boost::asio::awaitable<void> at_receive_negotiate(char command);
        virtual boost::asio::awaitable<void> at_receive_subnegotiate(std::string_view data);

        virtual boost::asio::awaitable<void> at_local_reject();
        virtual boost::asio::awaitable<void> at_remote_reject();
        virtual boost::asio::awaitable<void> at_local_enable();
        virtual boost::asio::awaitable<void> at_remote_enable();
        virtual boost::asio::awaitable<void> at_local_disable();
        virtual boost::asio::awaitable<void> at_remote_disable();

        TelnetOptionState local;   // our side
        TelnetOptionState remote;  // their side

        private:
        TelnetConnection& tc;

        std::shared_ptr<Channel<bool>> getPendingChannel(const std::string& name);
        std::unordered_map<std::string, std::shared_ptr<Channel<bool>>> pending_channels_;

    };
}