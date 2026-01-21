#include "telnet/telnet.hpp"
#include <type_traits>

namespace vol::telnet
{

    TelnetMessage parseSubNegotiation(char option, std::string_view data)
    {
        using namespace codes;
        switch (option)
        {
        case MSSP:
        {
            // MSSP: IAC SB MSSP (VAR value VAL value)* IAC SE
            // VAR = 1, VAL = 2
            std::vector<std::pair<std::string, std::string>> variables;
            size_t pos = 0;
            std::string current_name;

            while (pos < data.size())
            {
                unsigned char marker = static_cast<unsigned char>(data[pos]);
                if (marker == 1) { // VAR
                    pos++;
                    size_t start = pos;
                    while (pos < data.size() && static_cast<unsigned char>(data[pos]) != 2 && static_cast<unsigned char>(data[pos]) != 1) {
                        pos++;
                    }
                    current_name = std::string(data.substr(start, pos - start));
                } else if (marker == 2) { // VAL
                    pos++;
                    size_t start = pos;
                    while (pos < data.size() && static_cast<unsigned char>(data[pos]) != 1 && static_cast<unsigned char>(data[pos]) != 2) {
                        pos++;
                    }
                    auto value = std::string(data.substr(start, pos - start));
                    if (!current_name.empty()) {
                        variables.emplace_back(current_name, value);
                        current_name.clear();
                    }
                } else {
                    // skip unknown byte
                    pos++;
                }
            }
            return TelnetMessageMSSP{variables};
        }
        case NAWS:
        {
            if (data.size() >= 4)
            {
                uint16_t width = (static_cast<uint8_t>(data[0]) << 8) | static_cast<uint8_t>(data[1]);
                uint16_t height = (static_cast<uint8_t>(data[2]) << 8) | static_cast<uint8_t>(data[3]);
                return TelnetMessageNAWS{width, height};
            }
            return TelnetMessageNAWS{80, 24}; // default size
        }
        case MCCP2:
        case MCCP3:
        {
            // Compression negotiation
            return TelnetMessageCompress{option, true};
        }
        case GMCP:
        {
            // GMCP data is in the format <package> [<json>]
            // meaning, if there is a space in the data, the first part is the package name,
            // and the rest is a JSON string.
            // but if there's no space, it's just the package name with no data.
            size_t space_pos = data.find(' ');
            if (space_pos == std::string_view::npos)
            {
                // no data
                return TelnetMessageGMCP{std::string(data), nlohmann::json::object()};
            }
            else
            {
                auto package = data.substr(0, space_pos);
                auto json_str = data.substr(space_pos + 1);
                try
                {
                    nlohmann::json j = nlohmann::json::parse(json_str);
                    return TelnetMessageGMCP{std::string(package), j};
                }
                catch (...)
                {
                    return TelnetMessageGMCP{std::string(package), nlohmann::json::object()};
                }
            }
        }
        case TERMINAL_TYPE:
        {
            // we need at least 1 byte of data for the terminal type command.
            if (data.size() < 1)
            {
                return TelnetError{"Invalid TERMINAL TYPE subnegotiation - missing command"};
            }
            if (data[0] == 0)
            {
                // IS command
                auto term_type = data.substr(1);
                return TelnetMessageMTTS{std::string(term_type)};
            }
            else if (data[0] == 1)
            {
                // SEND command - we don't expect to receive this from the client
                return TelnetError{"Unexpected SEND command in TERMINAL TYPE subnegotiation"};
            }
            else
            {
                return TelnetError{"Unknown command in TERMINAL TYPE subnegotiation"};
            }
        }
        case CHARSET: {
            // CHARSET negotiation
            if (data.size() < 1)
            {
                return TelnetError{"Invalid CHARSET subnegotiation - missing command"};
            }
            char command = data[0];
            auto charsets = data.substr(1);
            return TelnetMessageCharset{command, std::string(charsets)};
        }
        default:
            return TelnetMessageSubnegotiation{option, std::string(data)}; // unhandled, return as-is
        }
    }

    std::expected<std::pair<TelnetMessage, size_t>, std::string> parseTelnetMessage(std::string_view data)
    {
        if (data.empty())
        {
            return std::unexpected("No data to parse");
        }

        auto avail = data.size();

        if (data[0] == codes::IAC)
        {
            // we're doing an IAC sequence.
            if (avail < 2)
            {
                return std::unexpected("Incomplete IAC sequence - need at least 2 bytes");
            }

            switch (data[1])
            {
            case codes::WILL:
            case codes::WONT:
            case codes::DO:
            case codes::DONT:
            {
                if (avail < 3)
                {
                    return std::unexpected("Incomplete negotiation sequence - need at least 3 bytes");
                }
                TelnetMessage msg = TelnetMessageNegotiation{data[1], data[2]};
                return std::make_pair(msg, 3);
            }

            case codes::SB:
            {
                // subnegotiation: IAC SB <op> [<data>] IAC SE
                if (avail < 5)
                {
                    return std::unexpected("Incomplete subnegotiation sequence - need at least 5 bytes");
                }
                auto op = data[2];
                // we know that we start with IAC SB <op>... now we need to scan until we find an unescaped IAC SE
                size_t pos = 3;
                while (pos + 1 < avail)
                {
                    if (data[pos] == codes::IAC)
                    {
                        if (data[pos + 1] == codes::SE)
                        {
                            // end of subnegotiation
                            std::string sub_data;
                            if (pos > 3)
                            {
                                sub_data.reserve(pos - 3);
                                size_t i = 3;
                                while (i < pos) {
                                    if (data[i] == codes::IAC && i + 1 < pos && data[i + 1] == codes::IAC) {
                                        sub_data.push_back(codes::IAC);
                                        i += 2;
                                    } else {
                                        sub_data.push_back(data[i]);
                                        i += 1;
                                    }
                                }
                            }
                            auto translated = TelnetMessageSubnegotiation{op, sub_data};
                            return std::make_pair(translated, pos + 2);
                        }
                        else if (data[pos + 1] == codes::IAC)
                        {
                            // escaped 255 byte, skip it
                            pos += 2;
                        }
                        else
                        {
                            // something else - just continue
                            pos += 1;
                        }
                    }
                    else
                    {
                        pos += 1;
                    }
                }
                return std::unexpected("Incomplete subnegotiation sequence - missing IAC SE terminator");
            }
            case codes::IAC:
            {
                // escaped 255 data byte
                TelnetMessage msg = TelnetMessageData{std::string(1, codes::IAC)};
                return std::make_pair(msg, 2);
            }
            default:
            {
                // command
                TelnetMessage msg = TelnetMessageCommand{data[1]};
                return std::make_pair(msg, 2);
            }
            }
        }
        else
        {
            // regular data
            size_t pos = data.find(codes::IAC);
            if (pos == std::string_view::npos)
            {
                pos = data.size();
            }
            TelnetMessage msg = TelnetMessageData{std::string(data.substr(0, pos))};
            return std::make_pair(msg, pos);
        }
    }

    static void append_iac_escaped(std::string& out, std::string_view data) {
        for (char ch : data) {
            out.push_back(ch);
            if (static_cast<unsigned char>(ch) == static_cast<unsigned char>(codes::IAC)) {
                out.push_back(codes::IAC);
            }
        }
    }

    static void append_subnegotiation(std::string& out, char option, std::string_view data) {
        out.push_back(codes::IAC);
        out.push_back(codes::SB);
        out.push_back(option);
        append_iac_escaped(out, data);
        out.push_back(codes::IAC);
        out.push_back(codes::SE);
    }

    std::string encodeTelnetMessage(const TelnetMessage& msg) {
        return std::visit([](const auto& m) -> std::string {
            using T = std::decay_t<decltype(m)>;
            std::string out;

            if constexpr (std::is_same_v<T, TelnetMessageData>) {
                out = m.data;
            } else if constexpr (std::is_same_v<T, TelnetMessageNegotiation>) {
                out.push_back(codes::IAC);
                out.push_back(m.command);
                out.push_back(m.option);
            } else if constexpr (std::is_same_v<T, TelnetMessageCommand>) {
                out.push_back(codes::IAC);
                out.push_back(m.command);
            } else if constexpr (std::is_same_v<T, TelnetMessageSubnegotiation>) {
                append_subnegotiation(out, m.option, m.data);
            } else if constexpr (std::is_same_v<T, TelnetMessageMSSP>) {
                std::string payload;
                for (const auto& [key, value] : m.variables) {
                    payload.push_back(static_cast<char>(1));
                    payload.append(key);
                    payload.push_back(static_cast<char>(2));
                    payload.append(value);
                }
                append_subnegotiation(out, codes::MSSP, payload);
            } else if constexpr (std::is_same_v<T, TelnetMessageNAWS>) {
                std::string payload;
                payload.resize(4);
                payload[0] = static_cast<char>((m.width >> 8) & 0xFF);
                payload[1] = static_cast<char>(m.width & 0xFF);
                payload[2] = static_cast<char>((m.height >> 8) & 0xFF);
                payload[3] = static_cast<char>(m.height & 0xFF);
                append_subnegotiation(out, codes::NAWS, payload);
            } else if constexpr (std::is_same_v<T, TelnetMessageCompress>) {
                if (m.state) {
                    append_subnegotiation(out, m.version, "");
                }
            } else if constexpr (std::is_same_v<T, TelnetMessageGMCP>) {
                std::string payload = m.package;
                if (!m.data.is_null() && !m.data.empty()) {
                    payload.push_back(' ');
                    payload.append(m.data.dump());
                }
                append_subnegotiation(out, codes::GMCP, payload);
            } else if constexpr (std::is_same_v<T, TelnetMessageMTTS>) {
                std::string payload;
                payload.push_back(static_cast<char>(0));
                payload.append(m.data);
                append_subnegotiation(out, codes::TERMINAL_TYPE, payload);
            } else if constexpr (std::is_same_v<T, TelnetMessageCharset>) {
                std::string payload;
                payload.push_back(m.command);
                payload.append(m.charsets);
                append_subnegotiation(out, codes::CHARSET, payload);
            } else if constexpr (std::is_same_v<T, TelnetError>) {
                out.clear();
            }

            return out;
        }, msg);
    }

}
