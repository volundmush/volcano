#pragma once
#include <string>
#include <string_view>
#include <variant>
#include <expected>
#include <vector>
#include <cstdint>
#include <functional>
#include <chrono>
#include <atomic>

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>

#include "net/Connection.hpp"

#include "mud/ClientData.hpp"

namespace vol::telnet {

    template<typename T>
    using Channel = boost::asio::experimental::concurrent_channel<void(boost::system::error_code, T)>;

    using VoidToken = std::monostate;

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
        constexpr char TERMINAL_TYPE = static_cast<char>(24);
        constexpr char TELOPT_EOR     = static_cast<char>(25);
        constexpr char NAWS          = static_cast<char>(31);
        constexpr char LINEMODE      = static_cast<char>(34);
        constexpr char MNES         = static_cast<char>(39);
        constexpr char CHARSET      = static_cast<char>(42);
        constexpr char MSSP          = static_cast<char>(70);
        constexpr char MCCP1         = static_cast<char>(85);
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
        std::unordered_map<std::string, std::string> variables;
        TelnetMessageSubnegotiation toSubnegotiation() const;
    };

    struct TelnetChangeCapabilities {
        nlohmann::json capabilities;
    };

    struct TelnetError {
        std::string message;
    };
    
    using TelnetMessage = std::variant<TelnetMessageData, TelnetMessageSubnegotiation, 
        TelnetMessageNegotiation, TelnetMessageCommand, TelnetError>;
    
    using TelnetToGameMessage = std::variant<TelnetMessageData, TelnetMessageGMCP, TelnetChangeCapabilities>;
    using TelnetFromGameMessage = std::variant<TelnetMessageData, TelnetMessageGMCP, TelnetMessageMSSP>;

    // attempts to parse a telnet message from the given data buffer.
    // On success, returns the parsed TelnetMessage and the number of bytes consumed.
    // On failure, returns an error string.
    std::expected<std::pair<TelnetMessage, size_t>, std::string> parseTelnetMessage(std::string_view data);

    // encodes a telnet message into bytes.
    std::string encodeTelnetMessage(const TelnetMessage& msg);

    class TelnetConnection;
    class TelnetOption;

    struct TelnetOptionState {
        bool enabled{false};
        bool negotiating{false};
        bool supported{false};
        bool start{false};
    };


    class TelnetOption {
        public:
        TelnetOption(TelnetConnection& connection);
        virtual ~TelnetOption() = default;

        Channel<TelnetToGameMessage> to_game;
        Channel<TelnetFromGameMessage> from_game;
        
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

        virtual boost::asio::awaitable<std::string> transform_outgoing_data(std::string_view& data);
        virtual boost::asio::awaitable<std::string> transform_incoming_data(std::string_view& data);

        TelnetOptionState local;   // our side
        TelnetOptionState remote;  // their side

        virtual bool shouldTransformIncomingData();
        virtual int shouldTransformIncomingPriority();

        virtual bool shouldTransformOutgoingData();
        virtual int shouldTransformOutgoingPriority();

        private:
        TelnetConnection& tc;
        char option_code_;
        std::shared_ptr<Channel<VoidToken>> getPendingChannel(const std::string& name);
        std::unordered_map<std::string, std::shared_ptr<Channel<VoidToken>>> pending_channels_;
        

    };

    class SGAOption : public TelnetOption {
        
    };

    class TelnetConnection {
        public:
        TelnetConnection(vol::net::AnyStream connection);
        
        boost::asio::awaitable<void> run();
        void requestAbort();

        const vol::mud::ClientData& client_data() const {
            return client_data_;
        }

        Channel<TelnetToGameMessage>& to_game_channel() {
            return to_game_messages_;
        }

        boost::asio::awaitable<void> sendToClient(const TelnetFromGameMessage& msg);

        private:
        vol::net::AnyStream conn_;
        vol::mud::ClientData client_data_;
        std::vector<std::shared_ptr<Channel<VoidToken>>> pending_channels_;
        Channel<TelnetMessage> outgoing_messages_;
        Channel<TelnetToGameMessage> to_game_messages_;
        std::atomic_bool abort_requested_{false};
        std::string append_data_buffer_;
        
        boost::asio::awaitable<void> runReader();
        boost::asio::awaitable<void> runWriter();
        boost::asio::awaitable<void> negotiateOptions(boost::asio::steady_timer::duration negotiation_timeout);
        boost::asio::awaitable<void> processData(TelnetMessage& data);

        boost::asio::awaitable<void> sendAppData(std::string_view app_data);
        boost::asio::awaitable<void> sendSubNegotiation(char option, std::string_view sub_data);
        boost::asio::awaitable<void> sendNegotiation(char command, char option);

        boost::asio::awaitable<void> notifyChangedCapabilities(nlohmann::json& capabilities);

        std::unordered_map<char, std::shared_ptr<TelnetOption>> option_states_;

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

}
