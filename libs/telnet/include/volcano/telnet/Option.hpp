#pragma once

#include "Base.hpp"
#include <unordered_map>

namespace volcano::mud {
    struct ClientData;
}

namespace volcano::telnet {

    struct TelnetOptionState {
        bool enabled{false};
        bool negotiating{false};
    };

    class TelnetOption {
        public:
        TelnetOption(TelnetConnection& connection);
        virtual ~TelnetOption() = default;

        virtual char option_code() const = 0;
        
        virtual boost::asio::awaitable<void> start();


        virtual boost::asio::awaitable<void> at_receive_negotiate(char command);
        virtual boost::asio::awaitable<void> at_receive_subnegotiate(std::string_view data);

        protected:
        TelnetConnection& tc;

        TelnetOptionState local;   // our side
        TelnetOptionState remote;  // their side

        std::shared_ptr<Channel<bool>> getPendingChannel(const std::string& name);
        std::unordered_map<std::string, std::shared_ptr<Channel<bool>>> pending_channels_;
        virtual std::string getBaseChannelName() = 0;

        boost::asio::awaitable<void> markNegotiationComplete(std::string name);

        volcano::mud::ClientData& client_data();
        boost::asio::awaitable<void> notifyChangedCapabilities(nlohmann::json& capabilities);

        // supported, auto-start, in order.
        virtual std::pair<bool, bool> getLocalSupportInfo();
        virtual std::pair<bool, bool> getRemoteSupportInfo();

        // overloadable hooks for state changes.
        virtual boost::asio::awaitable<void> at_local_reject();
        virtual boost::asio::awaitable<void> at_remote_reject();
        virtual boost::asio::awaitable<void> at_local_enable();
        virtual boost::asio::awaitable<void> at_remote_enable();
        virtual boost::asio::awaitable<void> at_local_disable();
        virtual boost::asio::awaitable<void> at_remote_disable();

        virtual boost::asio::awaitable<void> send_negotiation(char command);
        virtual boost::asio::awaitable<void> send_subnegotiate(std::string_view data);

        virtual boost::asio::awaitable<void> at_send_negotiate(char command);
        virtual boost::asio::awaitable<void> at_send_subnegotiate(std::string_view data);
    };

    class NAWSOption : public TelnetOption {
        public:
        using TelnetOption::TelnetOption;
        char option_code() const override;
        std::string getBaseChannelName() override;
        std::pair<bool, bool> getRemoteSupportInfo() override;
        boost::asio::awaitable<void> at_remote_enable() override;
        boost::asio::awaitable<void> at_receive_subnegotiate(std::string_view data) override;
    };

}