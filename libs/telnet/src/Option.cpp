#include "volcano/telnet/Option.hpp"
#include "volcano/telnet/Connection.hpp"

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


}