#pragma once
#include <string>
#include <string_view>
#include <variant>
#include <expected>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <chrono>
#include <atomic>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>

#include "volcano/mud/ClientData.hpp"

namespace volcano::telnet {
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
        constexpr char MTTS          = static_cast<char>(24);
        constexpr char TELOPT_EOR     = static_cast<char>(25);
        constexpr char NAWS          = static_cast<char>(31);
        constexpr char LINEMODE      = static_cast<char>(34);
        constexpr char MNES         = static_cast<char>(39);
        constexpr char CHARSET      = static_cast<char>(42);
        constexpr char MSSP          = static_cast<char>(70);
        constexpr char MCCP2         = static_cast<char>(86);
        constexpr char MCCP3         = static_cast<char>(87);
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
    
    using TelnetMessage = std::variant<TelnetMessageData, TelnetMessageSubnegotiation, 
        TelnetMessageNegotiation, TelnetMessageCommand, TelnetMessageGMCP>;
    
    using TelnetGameMessage = std::variant<TelnetMessageData, TelnetMessageGMCP, TelnetChangeCapabilities>;
    using TelnetClientMessage = std::variant<TelnetMessageData, TelnetMessageGMCP, TelnetMessageMSSP>;

    enum class TelnetDisconnect {
        unknown,
        remote_disconnect,
        local_disconnect,
        buffer_overflow,
        appdata_overflow,
        protocol_error,
        error,
    };
    enum class TelnetShutdownReason {
        unknown,
        client_disconnect,
        remote_disconnect,
        aborted,
        error,
    };

    using TelnetOutgoingMessage = std::variant<TelnetMessage, TelnetDisconnect>;
    using TelnetToGameMessage = std::variant<TelnetGameMessage, TelnetDisconnect>;
    using TelnetToTelnetMessage = std::variant<TelnetClientMessage, TelnetDisconnect>;

    struct TelnetLimits {
        std::size_t max_message_buffer{2 * 1024 * 1024};
        std::size_t max_appdata_buffer{64 * 1024};
    };

    extern TelnetLimits telnet_limits;

    struct TelnetLink {
        std::int64_t connection_id{0};
        boost::asio::ip::address address;
        std::string hostname;
        volcano::mud::ClientData client_data;
        std::shared_ptr<Channel<TelnetToGameMessage>> to_game;
        std::shared_ptr<Channel<TelnetToTelnetMessage>> to_telnet;
    };

    inline auto format_as(const TelnetLink& telnet_link) {
        return fmt::format("TelnetLink#{}({})", telnet_link.connection_id, telnet_link.address.to_string());
    }

    class TelnetConnection;
    class TelnetOption;
}