module;

#include <atomic>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <fmt/format.h>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/steady_timer.hpp>

export module volcano.telnet:base;

import volcano.client;
import volcano.net;
import nlohmann.json;

export namespace volcano::telnet {
    template<typename T>
    using Channel = boost::asio::experimental::concurrent_channel<void(boost::system::error_code, T)>;

    namespace codes {
        constexpr char NUL = static_cast<char>(0);
        constexpr char SGA = static_cast<char>(3);
        constexpr char BEL = static_cast<char>(7);
        constexpr char IAC  = static_cast<char>(255);
        constexpr char DONT = static_cast<char>(254);
        constexpr char DO   = static_cast<char>(253);
        constexpr char WONT = static_cast<char>(252);
        constexpr char WILL = static_cast<char>(251);
        constexpr char SB   = static_cast<char>(250);
        constexpr char SE   = static_cast<char>(240);

        // Telnet Commands
        constexpr char NOP = static_cast<char>(241);
        constexpr char AYT = static_cast<char>(246);

        // Telnet Options
        constexpr char MTTS = static_cast<char>(24);
        constexpr char TELOPT_EOR     = static_cast<char>(25);
        constexpr char NAWS          = static_cast<char>(31);
        constexpr char LINEMODE      = static_cast<char>(34);
        constexpr char MNES         = static_cast<char>(39);
        constexpr char CHARSET      = static_cast<char>(42);
        constexpr char MSSP          = static_cast<char>(70);
        constexpr char MCCP2         = static_cast<char>(86);
        constexpr char MCCP3         = static_cast<char>(87);
        constexpr char MXP          = static_cast<char>(91);
        constexpr char GMCP          = static_cast<char>(201);
    }

    struct TelnetMessageData {
        std::string data;
    };

    struct TelnetMessageSubnegotiation {
        char option; // e.g., TERMINAL TYPE, MCCP, etc.
        std::string data;
    };

    struct TelnetMessageNegotiation {
        char command; // WILL, WONT, DO, DONT
        char option;
    };

    struct TelnetMessageCommand {
        char command; // e.g., NOP, AYT, etc.
    };

    struct TelnetMessageGMCP {
        std::string package;
        nlohmann::json data;
        TelnetMessageSubnegotiation toSubnegotiation() const;
    };

    struct TelnetMessageMSSP {
        std::vector<std::pair<std::string, std::string>> variables;
        TelnetMessageSubnegotiation toSubnegotiation() const;
    };

    struct TelnetChangeCapabilities {
        nlohmann::json capabilities;
    };

    struct TelnetError {
        std::string message;
    };

    using TelnetMessage = std::variant<TelnetMessageData, TelnetMessageSubnegotiation,
        TelnetMessageNegotiation, TelnetMessageCommand, TelnetMessageGMCP, TelnetError>;

    using TelnetToGameMessage = std::variant<TelnetMessageData, TelnetMessageGMCP, TelnetChangeCapabilities>;
    using TelnetFromGameMessage = std::variant<TelnetMessageData, TelnetMessageGMCP, TelnetMessageMSSP>;

    class TelnetConnection;

    struct TelnetOptionState {
        bool enabled{false};
        bool negotiating{false};
    };

    class TelnetOption {
        public:
        explicit TelnetOption(TelnetConnection& connection);
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
        std::pair<bool, bool> getLocalSupportInfo() override;
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
        std::pair<bool, bool> getRemoteSupportInfo() override;
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

    class TelnetConnection {
        friend class TelnetOption;

        public:
        explicit TelnetConnection(volcano::net::AnyStream connection);

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
        bool telnet_mode{false};

        boost::asio::awaitable<void> runReader();
        boost::asio::awaitable<void> runWriter();
        boost::asio::awaitable<void> runKeepAlive();

        boost::asio::awaitable<void> processData(TelnetMessage& data);

        boost::asio::awaitable<void> sendAppData(std::string_view app_data);
        boost::asio::awaitable<void> sendSubNegotiation(char option, std::string_view sub_data);
        boost::asio::awaitable<void> sendNegotiation(char command, char option);
        boost::asio::awaitable<void> sendCommand(char command);
        boost::asio::awaitable<void> notifyChangedCapabilities(nlohmann::json& capabilities);

        std::unordered_map<char, std::shared_ptr<TelnetOption>> options_;

        // event handlers
        boost::asio::awaitable<void> handleAppData(TelnetMessageData& app_data);
        boost::asio::awaitable<void> handleNegotiate(TelnetMessageNegotiation& negotiation);
        boost::asio::awaitable<void> handleSubNegotiation(TelnetMessageSubnegotiation& subnegotiation);
        boost::asio::awaitable<void> handleCommand(TelnetMessageCommand& command);
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
}

namespace volcano::telnet {
    TelnetMessageSubnegotiation TelnetMessageGMCP::toSubnegotiation() const {
        std::string payload = package;
        if (!data.is_null()) {
            payload.push_back(' ');
            payload += data.dump();
        }
        return TelnetMessageSubnegotiation{codes::GMCP, std::move(payload)};
    }

    TelnetMessageSubnegotiation TelnetMessageMSSP::toSubnegotiation() const {
        std::string payload;
        for (const auto& [key, value] : variables) {
            payload.push_back(static_cast<char>(1));
            payload += key;
            payload.push_back(static_cast<char>(2));
            payload += value;
        }
        return TelnetMessageSubnegotiation{codes::MSSP, std::move(payload)};
    }
}
