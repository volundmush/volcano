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

#include <boost/asio/experimental/concurrent_channel.hpp>

namespace vol::telnet {
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
        constexpr char TERMINAL_TYPE = static_cast<char>(24);
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
        TelnetMessageNegotiation, TelnetMessageCommand, TelnetError>;
    
    using TelnetToGameMessage = std::variant<TelnetMessageData, TelnetMessageGMCP, TelnetChangeCapabilities>;
    using TelnetFromGameMessage = std::variant<TelnetMessageData, TelnetMessageGMCP, TelnetMessageMSSP>;

    class TelnetConnection;
    class TelnetOption;
}