#include "volcano/telnet/Option.hpp"
#include "volcano/telnet/Connection.hpp"
#include "volcano/log/Log.hpp"

#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <cctype>

namespace volcano::telnet {

    TelnetOption::TelnetOption(TelnetConnection& connection) : tc(connection) {
    }

    boost::asio::awaitable<void> TelnetOption::start() {
        getPendingChannel(getBaseChannelName());

        auto local_info = getLocalSupportInfo();
        auto remote_info = getRemoteSupportInfo();

        if(local_info.first && local_info.second) {
            co_await send_negotiation(codes::WILL);
            local.negotiating = true;
        }

        if(remote_info.first && remote_info.second) {
            co_await send_negotiation(codes::DO);
            remote.negotiating = true;
        }

        co_return;
    }

    boost::asio::awaitable<void> TelnetOption::send_negotiation(char command) {
        co_await tc.sendNegotiation(command, option_code());
        co_await at_send_negotiate(command);
        co_return;
    }

    boost::asio::awaitable<void> TelnetOption::send_subnegotiate(std::string_view data) {
        co_await tc.sendSubNegotiation(option_code(), data);
        co_await at_send_subnegotiate(data);
        co_return;
    }

    boost::asio::awaitable<void> TelnetOption::at_send_negotiate(char command) {
        co_return;
    }

    boost::asio::awaitable<void> TelnetOption::at_send_subnegotiate(std::string_view data) {
        co_return;
    }

    boost::asio::awaitable<void> TelnetOption::at_receive_negotiate(char command) {
        auto local_info = getLocalSupportInfo();
        auto remote_info = getRemoteSupportInfo();

        switch(command) {
            case codes::WILL: {
                if(remote_info.first) {
                    // supported...
                    if(!remote.enabled) {
                        remote.enabled = true;
                        if(!remote.negotiating) {
                            co_await send_negotiation(codes::DO);
                        }
                        co_await at_remote_enable();
                    }
                } else {
                    co_await send_negotiation(codes::DONT);
                    co_await at_remote_reject();
                }
            }
            break;
            case codes::DO: {
                if(local_info.first) {
                    // supported...
                    if(!local.enabled) {
                        local.enabled = true;
                        if(!local.negotiating) {
                            co_await send_negotiation(codes::WILL);
                        }
                        co_await at_local_enable();
                    }
                } else {
                    co_await send_negotiation(codes::WONT);
                    co_await at_local_reject();
                }
            }
            break;
            case codes::WONT: {
                if(remote_info.first) {
                    if(remote.enabled) {
                        remote.enabled = false;
                        co_await at_remote_disable();
                    }
                    if(remote.negotiating) {
                        remote.negotiating = false;
                        co_await at_remote_reject();
                    }
                }
            }
            break;
            case codes::DONT: {
                if(local_info.first) {
                    if(local.enabled) {
                        local.enabled = false;
                        co_await at_local_disable();
                    }
                    if(local.negotiating) {
                        local.negotiating = false;
                        co_await at_local_reject();
                    }
                }
            }
            break;
            default:
                break;
        }

        co_return;
    }

    boost::asio::awaitable<void> TelnetOption::at_receive_subnegotiate(std::string_view data) {
        co_return;
    }

    boost::asio::awaitable<void> TelnetOption::at_local_reject() {
        co_await markNegotiationComplete(getBaseChannelName());
        co_return;
    }

    boost::asio::awaitable<void> TelnetOption::at_remote_reject() {
        co_await markNegotiationComplete(getBaseChannelName());
        co_return;
    }

    boost::asio::awaitable<void> TelnetOption::at_local_enable() {
        co_await markNegotiationComplete(getBaseChannelName());
        co_return;
    }

    boost::asio::awaitable<void> TelnetOption::at_remote_enable() {
        co_await markNegotiationComplete(getBaseChannelName());
        co_return;
    }

    boost::asio::awaitable<void> TelnetOption::at_local_disable() {
        co_return;
    }

    boost::asio::awaitable<void> TelnetOption::at_remote_disable() {
        co_return;
    }

    boost::asio::awaitable<void> TelnetOption::markNegotiationComplete(std::string name) {
        if(tc.is_negotiation_completed()) {
            co_return; // already completed no need to signal
        }
        auto channel = getPendingChannel(name);
        boost::system::error_code ec;
        co_await channel->async_send(ec, true, boost::asio::use_awaitable);
        co_return;
    }

    volcano::mud::ClientData& TelnetOption::client_data() {
        return tc.client_data_;
    }

    boost::asio::awaitable<void> TelnetOption::notifyChangedCapabilities(nlohmann::json& capabilities) {
        co_await tc.notifyChangedCapabilities(capabilities);
        co_return;
    }

    std::shared_ptr<Channel<bool>> TelnetOption::getPendingChannel(const std::string& name) {
        auto it = pending_channels_.find(name);
        if (it != pending_channels_.end()) {
            return it->second;
        }
        auto channel = std::make_shared<Channel<bool>>(tc.conn_.get_executor(), 1);
        pending_channels_[name] = channel;
        tc.pending_channels_.push_back(channel);
        return channel;
    }

    std::pair<bool, bool> TelnetOption::getLocalSupportInfo() {
        return {false, false};
    }

    std::pair<bool, bool> TelnetOption::getRemoteSupportInfo() {
        return {false, false};
    }

    // NAWS Section
    char NAWSOption::option_code() const {
        return codes::NAWS;
    }

    std::pair<bool, bool> NAWSOption::getRemoteSupportInfo() {
        // we always support NAWS, but do not auto-start it.
        return {true, true};
    }

    boost::asio::awaitable<void> NAWSOption::at_remote_enable() {
        co_await TelnetOption::markNegotiationComplete(getBaseChannelName());
        client_data().naws = true;
        nlohmann::json capabilities;
        capabilities["naws"] = true;
        co_await notifyChangedCapabilities(capabilities);
        co_return;
    }

    boost::asio::awaitable<void> NAWSOption::at_receive_subnegotiate(std::string_view data) {
        if(data.size() != 4) {
            co_return;
        }

        auto &cd = client_data();
        auto old_width = cd.width;
        auto old_height = cd.height;

        uint16_t width = (static_cast<uint8_t>(data[0]) << 8) | static_cast<uint8_t>(data[1]);
        uint16_t height = (static_cast<uint8_t>(data[2]) << 8) | static_cast<uint8_t>(data[3]);

        if(width == old_width && height == old_height) {
            co_return;
        }

        cd.width = width;
        cd.height = height;

        nlohmann::json capabilities;
        capabilities["width"] = width;
        capabilities["height"] = height;
        co_await notifyChangedCapabilities(capabilities);

        co_return;
    }

    std::string NAWSOption::getBaseChannelName() {
        return "NAWS";
    }

    // SGA Section
    char SGAOption::option_code() const {
        return codes::SGA;
    }

    std::string SGAOption::getBaseChannelName() {
        return "SGA";
    }

    std::pair<bool, bool> SGAOption::getLocalSupportInfo() {
        return {true, true};
    }

    boost::asio::awaitable<void> SGAOption::at_local_enable() {
        co_await TelnetOption::markNegotiationComplete(getBaseChannelName());
        client_data().sga = true;
        nlohmann::json capabilities;
        capabilities["sga"] = true;
        co_await notifyChangedCapabilities(capabilities);
        co_return;
    }

    // CHARSET Section
    char CHARSETOption::option_code() const {
        return codes::CHARSET;
    }

    std::string CHARSETOption::getBaseChannelName() {
        return "CHARSET";
    }

    std::pair<bool, bool> CHARSETOption::getRemoteSupportInfo() {
        return {true, true};
    }

    boost::asio::awaitable<void> CHARSETOption::request_charset() {
        std::string data;
        data.push_back(static_cast<char>(0x01));
        data += " ascii utf-8";
        co_await send_subnegotiate(data);
        co_return;
    }

    boost::asio::awaitable<void> CHARSETOption::at_remote_enable() {
        client_data().charset = true;
        if (!enabled_) {
            enabled_ = "remote";
            co_await request_charset();
        }
        co_return;
    }

    boost::asio::awaitable<void> CHARSETOption::at_local_enable() {
        client_data().charset = true;
        if (!enabled_) {
            enabled_ = "local";
            co_await request_charset();
        }
        co_return;
    }

    boost::asio::awaitable<void> CHARSETOption::at_receive_subnegotiate(std::string_view data) {
        if (data.size() < 2) {
            co_return;
        }

        if (static_cast<unsigned char>(data[0]) == 0x02) {
            std::string encoding(data.substr(1));
            client_data().encoding = encoding;
            nlohmann::json capabilities;
            capabilities["encoding"] = encoding;
            capabilities["charset"] = true;
            co_await notifyChangedCapabilities(capabilities);
            co_await markNegotiationComplete(getBaseChannelName());
        }
        co_return;
    }

    // MTTS Section
    char MTTSOption::option_code() const {
        return codes::MTTS;
    }

    std::string MTTSOption::getBaseChannelName() {
        return "MTTS";
    }

    std::pair<bool, bool> MTTSOption::getLocalSupportInfo() {
        return {true, true};
    }

    std::pair<bool, bool> MTTSOption::getRemoteSupportInfo() {
        return {true, true};
    }

    boost::asio::awaitable<void> MTTSOption::at_local_enable() {
        client_data().mtts = true;
        nlohmann::json capabilities;
        capabilities["mtts"] = true;
        co_await notifyChangedCapabilities(capabilities);
        if (number_requests_ == 0) {
            co_await request();
        }
        co_return;
    }

    boost::asio::awaitable<void> MTTSOption::at_remote_enable() {
        client_data().mtts = true;
        nlohmann::json capabilities;
        capabilities["mtts"] = true;
        co_await notifyChangedCapabilities(capabilities);
        if (number_requests_ == 0) {
            co_await request();
        }
        co_return;
    }

    boost::asio::awaitable<void> MTTSOption::request() {
        number_requests_ += 1;
        std::string data;
        data.push_back(static_cast<char>(0x01));
        co_await send_subnegotiate(data);
        co_return;
    }

    static std::string to_upper_copy(std::string_view input) {
        std::string out(input);
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return out;
    }

    boost::asio::awaitable<void> MTTSOption::handle_name(std::string_view data) {
        nlohmann::json out;

        std::string payload(data);
        auto space_pos = payload.find(' ');
        std::string client_name;
        std::string client_version = "UNKNOWN";

        if (space_pos != std::string::npos) {
            client_name = payload.substr(0, space_pos);
            client_version = payload.substr(space_pos + 1);
        } else {
            client_name = payload;
        }

        co_await tc.setClientName(client_name, client_version);

        co_return;
    }

    boost::asio::awaitable<void> MTTSOption::handle_ttype(std::string_view data) {
        std::string payload(data);
        auto dash_pos = payload.find('-');

        std::string first = (dash_pos == std::string::npos) ? payload : payload.substr(0, dash_pos);

        int max_color = client_data().color;

        std::string upper_first = to_upper_copy(first);
        if (max_color < 2) {
            if ((upper_first.size() >= 8 && upper_first.ends_with("-256COLOR")) ||
                (upper_first.ends_with("XTERM") && !upper_first.ends_with("-COLOR"))) {
                max_color = 2;
            }
        }

        nlohmann::json out;

        if (upper_first == "VT100") {
            client_data().vt100 = true;
            out["vt100"] = true;
        } else if (upper_first == "XTERM") {
            max_color = std::max(max_color, 2);
        }

        if (max_color != client_data().color) {
            client_data().color = static_cast<uint8_t>(max_color);
            out["color"] = max_color;
        }

        if (!out.empty()) {
            co_await notifyChangedCapabilities(out);
        }

        co_return;
    }

    boost::asio::awaitable<void> MTTSOption::handle_standard(std::string_view data) {
        if (!data.starts_with("MTTS ")) {
            co_return;
        }

        auto num_view = data.substr(5);
        int number = 0;
        try {
            number = std::stoi(std::string(num_view));
        } catch (...) {
            co_return;
        }

        struct MttsEntry { int bit; const char* name; };
        static constexpr MttsEntry mtts_values[] = {
            {2048, "encryption"}, {1024, "mslp"}, {512, "mnes"}, {256, "truecolor"},
            {128, "proxy"}, {64, "screenreader"}, {32, "osc_color_palette"}, {16, "mouse_tracking"},
            {8, "xterm256"}, {4, "utf8"}, {2, "vt100"}, {1, "ansi"}
        };

        nlohmann::json out;
        int max_color = client_data().color;

        for (const auto& entry : mtts_values) {
            if ((number & entry.bit) == 0) {
                continue;
            }

            std::string capability(entry.name);
            if (capability == "truecolor") {
                max_color = std::max(max_color, 3);
            } else if (capability == "xterm256") {
                max_color = std::max(max_color, 2);
            } else if (capability == "ansi") {
                max_color = std::max(max_color, 1);
            } else if (capability == "utf8") {
                client_data().encoding = "utf-8";
                out["encoding"] = "utf-8";
            } else if (capability == "screenreader") {
                client_data().screen_reader = true;
                out["screenreader"] = true;
            } else if (capability == "mouse_tracking") {
                client_data().mouse_tracking = true;
                out["mouse_tracking"] = true;
            } else if (capability == "osc_color_palette") {
                client_data().osc_color_palette = true;
                out["osc_color_palette"] = true;
            } else if (capability == "proxy") {
                client_data().proxy = true;
                out["proxy"] = true;
            } else if (capability == "vt100") {
                client_data().vt100 = true;
                out["vt100"] = true;
            } else if (capability == "mnes") {
                client_data().mnes = true;
                out["mnes"] = true;
            }
        }

        if (max_color != client_data().color) {
            client_data().color = static_cast<uint8_t>(max_color);
            out["color"] = max_color;
        }

        if (!out.empty()) {
            co_await notifyChangedCapabilities(out);
        }

        co_return;
    }

    boost::asio::awaitable<void> MTTSOption::at_receive_subnegotiate(std::string_view data) {
        if (data.empty()) {
            co_return;
        }
        if (static_cast<unsigned char>(data[0]) != 0) {
            co_return;
        }

        std::string payload(data.substr(1));
        if (payload == last_received_) {
            co_await markNegotiationComplete(getBaseChannelName());
            co_return;
        }
        last_received_ = payload;

        if (number_requests_ == 1) {
            co_await handle_name(payload);
            co_await request();
        } else if (number_requests_ == 2) {
            co_await handle_ttype(payload);
            co_await request();
        } else if (number_requests_ == 3) {
            co_await handle_standard(payload);
            co_await markNegotiationComplete(getBaseChannelName());
        }

        co_return;
    }

    // MSSP Section
    char MSSPOption::option_code() const {
        return codes::MSSP;
    }

    std::string MSSPOption::getBaseChannelName() {
        return "MSSP";
    }

    std::pair<bool, bool> MSSPOption::getLocalSupportInfo() {
        return {true, true};
    }

    boost::asio::awaitable<void> MSSPOption::at_local_enable() {
        co_await TelnetOption::markNegotiationComplete(getBaseChannelName());
        nlohmann::json capabilities;
        capabilities["mssp"] = true;
        co_await notifyChangedCapabilities(capabilities);
        co_return;
    }

    // MCCP2 Section
    char MCCP2Option::option_code() const {
        return codes::MCCP2;
    }

    std::string MCCP2Option::getBaseChannelName() {
        return "MCCP2";
    }

    std::pair<bool, bool> MCCP2Option::getLocalSupportInfo() {
        return {true, true};
    }

    boost::asio::awaitable<void> MCCP2Option::at_local_enable() {
        co_await TelnetOption::markNegotiationComplete(getBaseChannelName());
        client_data().mccp2 = true;
        nlohmann::json capabilities;
        capabilities["mccp2"] = true;
        co_await notifyChangedCapabilities(capabilities);
        co_await send_subnegotiate("");
        co_return;
    }

    boost::asio::awaitable<void> MCCP2Option::at_send_subnegotiate(std::string_view data) {
        if (!client_data().mccp2_enabled) {
            client_data().mccp2_enabled = true;
        }
        co_return;
    }

    // MCCP3 Section
    char MCCP3Option::option_code() const {
        return codes::MCCP3;
    }

    std::string MCCP3Option::getBaseChannelName() {
        return "MCCP3";
    }

    std::pair<bool, bool> MCCP3Option::getLocalSupportInfo() {
        return {true, true};
    }

    boost::asio::awaitable<void> MCCP3Option::at_local_enable() {
        co_await TelnetOption::markNegotiationComplete(getBaseChannelName());
        client_data().mccp3 = true;
        nlohmann::json capabilities;
        capabilities["mccp3"] = true;
        co_await notifyChangedCapabilities(capabilities);
        co_return;
    }

    boost::asio::awaitable<void> MCCP3Option::at_receive_subnegotiate(std::string_view data) {
        if (!client_data().mccp3_enabled) {
            client_data().mccp3_enabled = true;
            nlohmann::json capabilities;
            capabilities["mccp3_enabled"] = true;
            co_await notifyChangedCapabilities(capabilities);
        }
        co_return;
    }

    // GMCP Section
    char GMCPOption::option_code() const {
        return codes::GMCP;
    }

    std::string GMCPOption::getBaseChannelName() {
        return "GMCP";
    }

    std::pair<bool, bool> GMCPOption::getLocalSupportInfo() {
        return {true, true};
    }

    boost::asio::awaitable<void> GMCPOption::at_local_enable() {
        co_await TelnetOption::markNegotiationComplete(getBaseChannelName());
        client_data().gmcp = true;
        nlohmann::json capabilities;
        capabilities["gmcp"] = true;
        co_await notifyChangedCapabilities(capabilities);
        co_return;
    }

    boost::asio::awaitable<void> GMCPOption::at_receive_subnegotiate(std::string_view data) {
        std::string payload(data);
        std::string command;
        std::string json_payload;

        auto space_pos = payload.find(' ');
        if (space_pos != std::string::npos) {
            command = payload.substr(0, space_pos);
            json_payload = payload.substr(space_pos + 1);
        } else {
            command = payload;
        }

        nlohmann::json parsed;
        if (!json_payload.empty()) {
            try {
                parsed = nlohmann::json::parse(json_payload);
            } catch (...) {
                parsed = nullptr;
            }
        } else {
            parsed = nullptr;
        }

        if(boost::iequals(command, "Core.Hello")) {
            if(parsed.is_object()) {
                if(parsed.contains("client") && parsed["client"].is_string()) {
                    std::string client_name = parsed["client"];
                    std::string client_version = "UNKNOWN";
                    if(parsed.contains("version") && parsed["version"].is_string()) {
                        client_version = parsed["version"];
                    }
                    co_await tc.setClientName(client_name, client_version);
                }
            }
        }

        boost::system::error_code ec;
        co_await tc.to_game_channel().async_send(
            ec,
            TelnetMessageGMCP{std::move(command), std::move(parsed)},
            boost::asio::use_awaitable);
        if (ec) {
            LERROR("{} gmcp to_game channel error: {}", tc, ec.message());
        }
        co_return;
    }

    boost::asio::awaitable<void> GMCPOption::send_gmcp(std::string_view command, const nlohmann::json* data) {
        std::string out(command);
        if (data) {
            out.push_back(' ');
            out += data->dump();
        }
        co_await send_subnegotiate(out);
        co_return;
    }

    // Line Mode Section
    char LineModeOption::option_code() const {
        return codes::LINEMODE;
    }

    std::string LineModeOption::getBaseChannelName() {
        return "LINEMODE";
    }

    std::pair<bool, bool> LineModeOption::getLocalSupportInfo() {
        return {true, true};
    }

    boost::asio::awaitable<void> LineModeOption::at_local_enable() {
        co_await TelnetOption::markNegotiationComplete(getBaseChannelName());
        client_data().linemode = true;
        nlohmann::json capabilities;
        capabilities["linemode"] = true;
        co_await notifyChangedCapabilities(capabilities);
        co_return;
    }

    // EOR Section
    char EOROption::option_code() const {
        return codes::TELOPT_EOR;
    }

    std::string EOROption::getBaseChannelName() {
        return "EOR";
    }


}