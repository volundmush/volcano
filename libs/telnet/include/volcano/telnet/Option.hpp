#pragma once

#include "Base.hpp"
#include <unordered_map>
#include <optional>

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

    class SGAOption : public TelnetOption {
        public:
        using TelnetOption::TelnetOption;
        char option_code() const override;
        std::string getBaseChannelName() override;
        std::pair<bool, bool> getLocalSupportInfo() override;
        boost::asio::awaitable<void> at_local_enable() override;
    };

    class CHARSETOption : public TelnetOption {
        public:
        using TelnetOption::TelnetOption;
        char option_code() const override;
        std::string getBaseChannelName() override;
        std::pair<bool, bool> getRemoteSupportInfo() override;
        boost::asio::awaitable<void> at_remote_enable() override;
        boost::asio::awaitable<void> at_local_enable() override;
        boost::asio::awaitable<void> at_receive_subnegotiate(std::string_view data) override;

        private:
        boost::asio::awaitable<void> request_charset();
        std::optional<std::string> enabled_;
    };

    class MTTSOption : public TelnetOption {
        public:
        using TelnetOption::TelnetOption;
        char option_code() const override;
        std::string getBaseChannelName() override;
        std::pair<bool, bool> getLocalSupportInfo() override;
        std::pair<bool, bool> getRemoteSupportInfo() override;
        boost::asio::awaitable<void> at_local_enable() override;
        boost::asio::awaitable<void> at_remote_enable() override;
        boost::asio::awaitable<void> at_receive_subnegotiate(std::string_view data) override;

        private:
        boost::asio::awaitable<void> request();
        boost::asio::awaitable<void> handle_name(std::string_view data);
        boost::asio::awaitable<void> handle_ttype(std::string_view data);
        boost::asio::awaitable<void> handle_standard(std::string_view data);

        int number_requests_ = 0;
        std::string last_received_;
    };

    class MSSPOption : public TelnetOption {
        public:
        using TelnetOption::TelnetOption;
        char option_code() const override;
        std::string getBaseChannelName() override;
        std::pair<bool, bool> getLocalSupportInfo() override;
        boost::asio::awaitable<void> at_local_enable() override;
    };

    class MCCP2Option : public TelnetOption {
        public:
        using TelnetOption::TelnetOption;
        char option_code() const override;
        std::string getBaseChannelName() override;
        std::pair<bool, bool> getLocalSupportInfo() override;
        boost::asio::awaitable<void> at_local_enable() override;
        boost::asio::awaitable<void> at_send_subnegotiate(std::string_view data) override;
    };

    class MCCP3Option : public TelnetOption {
        public:
        using TelnetOption::TelnetOption;
        char option_code() const override;
        std::string getBaseChannelName() override;
        std::pair<bool, bool> getLocalSupportInfo() override;
        boost::asio::awaitable<void> at_local_enable() override;
        boost::asio::awaitable<void> at_receive_subnegotiate(std::string_view data) override;
    };

    class GMCPOption : public TelnetOption {
        public:
        using TelnetOption::TelnetOption;
        char option_code() const override;
        std::string getBaseChannelName() override;
        std::pair<bool, bool> getLocalSupportInfo() override;
        boost::asio::awaitable<void> at_local_enable() override;
        boost::asio::awaitable<void> at_receive_subnegotiate(std::string_view data) override;
        boost::asio::awaitable<void> send_gmcp(std::string_view command, const nlohmann::json* data = nullptr);
    };

    class LineModeOption : public TelnetOption {
        public:
        using TelnetOption::TelnetOption;
        char option_code() const override;
        std::string getBaseChannelName() override;
        std::pair<bool, bool> getLocalSupportInfo() override;
        boost::asio::awaitable<void> at_local_enable() override;
    };

    class EOROption : public TelnetOption {
        public:
        using TelnetOption::TelnetOption;
        char option_code() const override;
        std::string getBaseChannelName() override;
    };

}