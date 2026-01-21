#include "volcano/telnet/Connection.hpp"
#include "volcano/telnet/Option.hpp"
#include "volcano/log/Log.hpp"
#include "volcano/zlib/Zlib.hpp"

#include <cstddef>
#include <cstring>
#include <span>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/steady_timer.hpp>


namespace volcano::telnet {

    static std::expected<std::pair<TelnetMessage, size_t>, std::string> parseTelnetMessage(std::string_view data)
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

    static std::string encodeTelnetMessage(const TelnetMessage& msg) {
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
            } else if constexpr (std::is_same_v<T, TelnetError>) {
                out.clear();
            }

            return out;
        }, msg);
    }

    TelnetConnection::TelnetConnection(volcano::net::AnyStream connection)
        : conn_(std::move(connection)), 
        outgoing_messages_(conn_.get_executor(), 100),
        to_game_messages_(conn_.get_executor(), 100) {
            client_data_.connection_id = conn_.id();
            client_data_.tls = conn_.is_tls();
            client_data_.client_address = conn_.endpoint().address().to_string();
            client_data_.client_hostname = conn_.hostname();
            client_data_.client_protocol = "telnet";

            options_.emplace(codes::SGA, std::make_shared<SGAOption>(*this));
            options_.emplace(codes::NAWS, std::make_shared<NAWSOption>(*this));
            options_.emplace(codes::CHARSET, std::make_shared<CHARSETOption>(*this));
            options_.emplace(codes::MTTS, std::make_shared<MTTSOption>(*this));
            options_.emplace(codes::MSSP, std::make_shared<MSSPOption>(*this));
            options_.emplace(codes::MCCP2, std::make_shared<MCCP2Option>(*this));
            options_.emplace(codes::MCCP3, std::make_shared<MCCP3Option>(*this));
            options_.emplace(codes::GMCP, std::make_shared<GMCPOption>(*this));
            options_.emplace(codes::LINEMODE, std::make_shared<LineModeOption>(*this));
            options_.emplace(codes::TELOPT_EOR, std::make_shared<EOROption>(*this));

        }

    namespace {
        std::span<const std::byte> buffer_as_bytes(const boost::beast::flat_buffer& buffer) {
            auto data = buffer.data();
            return {
                reinterpret_cast<const std::byte*>(data.data()),
                data.size()
            };
        }

        void append_bytes(boost::beast::flat_buffer& buffer, std::span<const std::byte> chunk) {
            if (chunk.empty()) {
                return;
            }
            auto out = buffer.prepare(chunk.size());
            std::memcpy(out.data(), chunk.data(), chunk.size());
            buffer.commit(chunk.size());
        }
    }

    boost::asio::awaitable<void> TelnetConnection::runReader() {

        bool decompressing = false;
        volcano::zlib::InflateStream inflater;
        boost::beast::flat_buffer buffer, decompressed_buffer;

        try {
            
            while(true) {
                // we need to grab as many bytes as are available but not wait for more than that.
                boost::system::error_code read_ec;
                auto prepared = buffer.prepare(4096);
                std::size_t read_bytes = co_await conn_.async_read_some(
                    prepared,
                    boost::asio::redirect_error(boost::asio::use_awaitable, read_ec));
                if(read_ec) {
                    // TODO: proper error handling
                    LINFO("TelnetConnection read error with {}: {}", *this, read_ec.message());
                    co_return;
                }
                buffer.commit(read_bytes);
                if(buffer.size() == 0) {
                    continue;
                } else if(buffer.size() > 2 * 1024 * 1024) {
                    LERROR("{} received overly large message.", *this);
                    co_return;
                }

                if(decompressing) {
                    try {
                        auto input = buffer_as_bytes(buffer);
                        inflater.write(input, [&](std::span<const std::byte> chunk) {
                            append_bytes(decompressed_buffer, chunk);
                        });
                        buffer.consume(buffer.size());
                    } catch (const std::exception& e) {
                        LERROR("{} zlib inflate error {}", *this, e.what());
                        co_return;
                    }
                }

                boost::beast::flat_buffer& use_buffer = decompressing ? decompressed_buffer : buffer;

                auto parsed = parseTelnetMessage(
                    std::string_view{
                        static_cast<const char*>(use_buffer.data().data()),
                        use_buffer.size()
                    }
                );

                if(!parsed) {
                    continue;
                }

                use_buffer.consume(parsed.value().second);

                auto &msg = parsed.value().first;

                if(std::holds_alternative<TelnetMessageSubnegotiation>(msg)) {
                    auto& sub = std::get<TelnetMessageSubnegotiation>(msg);
                    if(sub.option == codes::MCCP3) {
                        // enable incoming decompression. We need to treat all further incoming data as compressed.
                        nlohmann::json capabilities;
                        capabilities["mccp3_enabled"] = true;
                        co_await notifyChangedCapabilities(capabilities);
                        decompressing = true;
                        inflater.reset();
                    }
                }

                co_await processData(msg);

            }
        } catch(boost::system::system_error& e) {
              LERROR("{} reader encountered an error: {}", conn_, e.what());
        }

        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::runWriter() {
        bool compressing = false;
        volcano::zlib::DeflateStream deflater(Z_BEST_COMPRESSION);
        boost::beast::flat_buffer buffer, compressed_buffer;

        try {
            for(;;) {
                boost::system::error_code ec;
                auto msg = co_await outgoing_messages_.async_receive(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec)
                );
                if(ec) {
                    LERROR("{} write channel error with: {}", *this, ec.message());
                    co_return;
                }

                auto encoded = encodeTelnetMessage(msg);
                if(encoded.empty()) {
                    continue;
                }

                buffer.consume(buffer.size());
                compressed_buffer.consume(compressed_buffer.size());

                auto dst = buffer.prepare(encoded.size());
                std::memcpy(dst.data(), encoded.data(), encoded.size());
                buffer.commit(encoded.size());

                boost::beast::flat_buffer& use_buffer = compressing ? compressed_buffer : buffer;
                if(compressing) {
                    try {
                        auto input = buffer_as_bytes(buffer);
                        deflater.write(input, [&](std::span<const std::byte> chunk) {
                            append_bytes(compressed_buffer, chunk);
                        }, volcano::zlib::FlushMode::sync);
                        buffer.consume(buffer.size());
                    } catch (const std::exception& e) {
                            LERROR("{} zlib deflate error {}", *this, e.what());
                        co_return;
                    }
                }

                boost::system::error_code write_ec;
                co_await boost::asio::async_write(
                    conn_,
                    use_buffer,
                    boost::asio::redirect_error(boost::asio::use_awaitable, write_ec));
                if(write_ec) {
                    LERROR("{} write error with: {}", *this, write_ec.message());
                    co_return;
                }

                if(std::holds_alternative<TelnetMessageSubnegotiation>(msg)) {
                    auto& sub = std::get<TelnetMessageSubnegotiation>(msg);
                    if(sub.option == codes::MCCP2) {
                        compressing = true;
                        nlohmann::json capabilities;
                        capabilities["mccp2_enabled"] = true;
                        co_await notifyChangedCapabilities(capabilities);
                        deflater.reset(Z_BEST_COMPRESSION);
                    }
                }
            }
        } catch(const boost::system::system_error& e) {
              LERROR("{} writer encountered an error: {}", *this, e.what());
        }

        co_return;
    }

    void TelnetConnection::requestAbort() {
        abort_requested_.store(true, std::memory_order_relaxed);
    }

    boost::asio::awaitable<void> TelnetConnection::negotiateOptions(boost::asio::steady_timer::duration negotiation_timeout) {
        using namespace boost::asio::experimental::awaitable_operators;

        auto exec = co_await boost::asio::this_coro::executor;
        boost::asio::steady_timer deadline(exec);
        deadline.expires_after(negotiation_timeout);

        auto wait_all = [this]() -> boost::asio::awaitable<void> {
            for(auto& chan : pending_channels_) {
                if(abort_requested_.load(std::memory_order_relaxed)) {
                    co_return;
                }
                if(!chan) {
                    continue;
                }

                boost::system::error_code recv_ec;
                co_await chan->async_receive(boost::asio::redirect_error(boost::asio::use_awaitable, recv_ec));
                if(recv_ec) {
                    LERROR("{} negotiation channel error: {}", *this, recv_ec.message());
                    co_return;
                }
            }

            co_return;
        };

        boost::system::error_code timer_ec;
        auto result = co_await (
            wait_all() ||
            deadline.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, timer_ec))
        );

        if(result.index() == 1) {
            co_return;
        }

        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::run() {
        using namespace boost::asio::experimental::awaitable_operators;

        for(auto& [code, option] : options_) {
            co_await option->start();
        }

        co_await (runReader() || runWriter());

        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::sendToClient(const TelnetFromGameMessage& msg) {
        TelnetMessage telnet_msg;
        if(std::holds_alternative<TelnetMessageData>(msg)) {
            TelnetMessageData data_msg = std::get<TelnetMessageData>(msg);
            telnet_msg = data_msg;
        } else if(std::holds_alternative<TelnetMessageGMCP>(msg)) {
            TelnetMessageGMCP gmcp_msg = std::get<TelnetMessageGMCP>(msg);
            telnet_msg = gmcp_msg.toSubnegotiation();
        } else if(std::holds_alternative<TelnetMessageMSSP>(msg)) {
            TelnetMessageMSSP mssp_msg = std::get<TelnetMessageMSSP>(msg);
            telnet_msg = mssp_msg.toSubnegotiation();
        } else {
            LERROR("{} sendToClient received unknown message variant.", *this);
            co_return;
        }
        
        boost::system::error_code ec;
        co_await outgoing_messages_.async_send(ec, std::move(telnet_msg), boost::asio::use_awaitable);
        if(ec) {
            LERROR("{} sendToClient channel error: {}", *this, ec.message());
        }
        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::handleAppData(TelnetMessageData& app_data) {
        append_data_buffer_ += app_data.data;

        auto send_line = [this](std::string line) -> boost::asio::awaitable<void> {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            boost::system::error_code ec;
            co_await to_game_messages_.async_send(ec, TelnetMessageData{std::move(line)}, boost::asio::use_awaitable);
            if(ec) {
                LERROR("{} to_game channel error: {}", conn_, ec.message());
            }
        };

        for (;;) {
            const auto pos = append_data_buffer_.find('\n');
            if (pos == std::string::npos) {
                break;
            }

            std::string line = append_data_buffer_.substr(0, pos);
            co_await send_line(std::move(line));
            append_data_buffer_.erase(0, pos + 1);
        }

        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::handleNegotiate(TelnetMessageNegotiation& negotiation) {
        telnet_mode = true;
        auto it = options_.find(negotiation.option);
        if(it != options_.end()) {
            co_await it->second->at_receive_negotiate(negotiation.command);
        } else {
            // by default, refuse all negotiations
            char response_command;
            switch(negotiation.command) {
                case codes::DO:
                    response_command = codes::WONT;
                    break;
                case codes::DONT:
                    response_command = codes::WONT;
                    break;
                case codes::WILL:
                    response_command = codes::DONT;
                    break;
                case codes::WONT:
                    response_command = codes::DONT;
                    break;
                default:
                    co_return;
            }
            co_await sendNegotiation(response_command, negotiation.option);
        }
        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::handleSubNegotiation(TelnetMessageSubnegotiation& subnegotiation) {
        telnet_mode = true;
        auto it = options_.find(subnegotiation.option);
        if(it != options_.end()) {
            co_await it->second->at_receive_subnegotiate(subnegotiation.data);
        } else {
            // unhandled subnegotiation
        }
        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::handleCommand(TelnetMessageCommand& command) {
        // Handle Telnet commands here if needed
        telnet_mode = true;
        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::processData(TelnetMessage& data) {
        if(std::holds_alternative<TelnetMessageData>(data)) {
            TelnetMessageData& msg = std::get<TelnetMessageData>(data);
            co_await handleAppData(msg);
        } else if(std::holds_alternative<TelnetMessageGMCP>(data)) {
            TelnetMessageGMCP& msg = std::get<TelnetMessageGMCP>(data);
            boost::system::error_code ec;
            co_await to_game_messages_.async_send(ec, TelnetMessageGMCP{std::move(msg.package), std::move(msg.data)}, boost::asio::use_awaitable);
            if(ec) {
                LERROR("{} to_game channel error: {}", *this, ec.message());
            }
        } else {
            // other message types are handled internally
        }

        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::sendAppData(std::string_view app_data) {
        boost::system::error_code ec;
        co_await outgoing_messages_.async_send(ec, TelnetMessageData{std::string(app_data)}, boost::asio::use_awaitable);
        if(ec) {
            LERROR("{} outgoing channel error: {}", *this, ec.message());
        }
        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::sendSubNegotiation(char option, std::string_view sub_data) {
        boost::system::error_code ec;
        co_await outgoing_messages_.async_send(ec, TelnetMessageSubnegotiation{option, std::string(sub_data)}, boost::asio::use_awaitable);
        if(ec) {
            LERROR("{} outgoing channel error: {}", *this, ec.message());
        }
        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::sendNegotiation(char command, char option) {
        boost::system::error_code ec;
        co_await outgoing_messages_.async_send(ec, TelnetMessageNegotiation{command, option}, boost::asio::use_awaitable);
        if(ec) {
            LERROR("{} outgoing channel error: {}", *this, ec.message());
        }
        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::sendCommand(char command) {
        boost::system::error_code ec;
        co_await outgoing_messages_.async_send(ec, TelnetMessageCommand{command}, boost::asio::use_awaitable);
        if(ec) {
            LERROR("{} outgoing channel error: {}", *this, ec.message());
        }
        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::runKeepAlive() {
        auto exec = co_await boost::asio::this_coro::executor;
        boost::asio::steady_timer keepalive_timer(exec);
        try {
            while(true) {
                keepalive_timer.expires_after(std::chrono::seconds(30));
                boost::system::error_code timer_ec;
                co_await keepalive_timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, timer_ec));
                if(timer_ec) {
                    LERROR("{} keepalive timer error: {}", *this, timer_ec.message());
                    co_return;
                }

                if(telnet_mode) {
                    co_await sendCommand(codes::NOP);
                }
            }
        } catch(const boost::system::system_error& e) {
              LERROR("{} keepalive encountered an error: {}", *this, e.what());
        }

        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::notifyChangedCapabilities(nlohmann::json& capabilities) {
        boost::system::error_code ec;
        co_await to_game_messages_.async_send(ec, TelnetChangeCapabilities{capabilities}, boost::asio::use_awaitable);
        if(ec) {
            LERROR("{} to_game channel error: {}", *this, ec.message());
        }
        co_return;
    }

}