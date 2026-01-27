#include "volcano/telnet/Connection.hpp"
#include "volcano/telnet/Option.hpp"
#include "volcano/log/Log.hpp"
#include "volcano/zlib/Zlib.hpp"
#include "volcano/net/net.hpp"

#include <cstddef>
#include <cstring>
#include <span>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
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
            }

            return out;
        }, msg);
    }

    TelnetConnection::TelnetConnection(volcano::net::AnyStream connection)
        : conn_(std::move(connection)), 
        outgoing_messages_(conn_.get_executor(), 100),
        to_telnet_messages_(std::make_shared<Channel<TelnetToTelnetMessage>>(conn_.get_executor(), 100)),
        to_game_messages_(std::make_shared<Channel<TelnetToGameMessage>>(conn_.get_executor(), 100)) {
            client_data_.tls = conn_.is_tls();
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
                    boost::asio::bind_cancellation_slot(
                        cancellation_signal_.slot(),
                        boost::asio::redirect_error(boost::asio::use_awaitable, read_ec)));
                if(read_ec) {
                    if(read_ec == boost::asio::error::operation_aborted) {
                        co_return;
                    }
                    LINFO("TelnetConnection read error with {}: {}", *this, read_ec.message());
                    boost::system::error_code send_ec;
                    co_await to_game_messages_->async_send(send_ec, TelnetDisconnect::remote_disconnect, boost::asio::use_awaitable);
                    if(send_ec) {
                        LERROR("{} to_game channel error: {}", *this, send_ec.message());
                    }
                    signalShutdown(TelnetShutdownReason::client_disconnect);
                    co_return;
                }
                buffer.commit(read_bytes);
                if(buffer.size() == 0) {
                    continue;
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

                if(use_buffer.size() > telnet_limits.max_message_buffer) {
                    LERROR("{} incoming buffer exceeded limit ({} bytes).", *this, telnet_limits.max_message_buffer);
                    co_await sendAppData("Input too large. Disconnecting.\r\n");
                    co_await sendToClient(TelnetDisconnect::buffer_overflow);
                    boost::system::error_code send_ec;
                    co_await to_game_messages_->async_send(send_ec, TelnetDisconnect::buffer_overflow, boost::asio::use_awaitable);
                    co_await volcano::net::waitForever(cancellation_signal_);
                    co_return;
                }

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
              signalShutdown(TelnetShutdownReason::error);
        }

        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::runLink() {
        try {
            co_await negotiateOptions(negotiation_timeout_);

            auto link = make_link();
            boost::system::error_code link_ec;
            co_await link_channel().async_send(link_ec, link, boost::asio::use_awaitable);
            if(link_ec) {
                LERROR("Telnet link channel error: {}", link_ec.message());
            }

            co_await volcano::net::waitForever(cancellation_signal_);

        } catch(const boost::system::system_error& e) {
            LERROR("{} runLink encountered an error: {}", *this, e.what());
            signalShutdown(TelnetShutdownReason::error);
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
                    boost::asio::bind_cancellation_slot(
                        cancellation_signal_.slot(),
                        boost::asio::redirect_error(boost::asio::use_awaitable, ec))
                );
                if(ec) {
                    if(ec == boost::asio::error::operation_aborted) {
                        co_return;
                    }
                    LERROR("{} write channel error with: {}", *this, ec.message());
                    signalShutdown(TelnetShutdownReason::error);
                    co_return;
                }

                if(std::holds_alternative<TelnetDisconnect>(msg)) {
                    auto reason = std::get<TelnetDisconnect>(msg);
                    switch (reason) {
                        case TelnetDisconnect::remote_disconnect:
                            signalShutdown(TelnetShutdownReason::remote_disconnect);
                            break;
                        case TelnetDisconnect::local_disconnect:
                            signalShutdown(TelnetShutdownReason::aborted);
                            break;
                        case TelnetDisconnect::buffer_overflow:
                        case TelnetDisconnect::appdata_overflow:
                        case TelnetDisconnect::protocol_error:
                        case TelnetDisconnect::error:
                        case TelnetDisconnect::unknown:
                            signalShutdown(TelnetShutdownReason::error);
                            break;
                    }
                    co_return;
                }

                auto &telnet_msg = std::get<TelnetMessage>(msg);
                auto encoded = encodeTelnetMessage(telnet_msg);
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
                    boost::asio::bind_cancellation_slot(
                        cancellation_signal_.slot(),
                        boost::asio::redirect_error(boost::asio::use_awaitable, write_ec)));
                if(write_ec) {
                    if(write_ec == boost::asio::error::operation_aborted) {
                        co_return;
                    }
                    LERROR("{} write error with: {}", *this, write_ec.message());
                    signalShutdown(TelnetShutdownReason::error);
                    co_return;
                }

                if(std::holds_alternative<TelnetMessageSubnegotiation>(telnet_msg)) {
                    auto& sub = std::get<TelnetMessageSubnegotiation>(telnet_msg);
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
              signalShutdown(TelnetShutdownReason::error);
        }

        co_return;
    }

    void TelnetConnection::requestAbort() {
        abort_requested_.store(true, std::memory_order_relaxed);
        signalShutdown(TelnetShutdownReason::aborted);
    }

    void TelnetConnection::signalShutdown(TelnetShutdownReason reason) {
        TelnetShutdownReason expected = TelnetShutdownReason::unknown;
        if(shutdown_reason_.compare_exchange_strong(expected, reason)) {
            cancellation_signal_.emit(boost::asio::cancellation_type::all);
        }
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

                    if(recv_ec == boost::asio::error::operation_aborted) {
                        co_return;
                    }

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

    boost::asio::awaitable<TelnetShutdownReason> TelnetConnection::run() {
        using namespace boost::asio::experimental::awaitable_operators;

        shutdown_reason_.store(TelnetShutdownReason::unknown, std::memory_order_relaxed);

        // first start all options. this will send initial negotiation messages as needed.
        for(auto& [code, option] : options_) {
            co_await option->start();
        }

        auto r = runReader();
        auto w = runWriter();
        auto k = runKeepAlive();
        auto l = runLink();

        co_await (std::move(r) || std::move(w) || std::move(k) || std::move(l));

        co_return shutdown_reason_.load(std::memory_order_relaxed);
    }


    boost::asio::awaitable<void> TelnetConnection::sendToClient(const TelnetToTelnetMessage& msg) {
        TelnetOutgoingMessage telnet_msg;
        if(std::holds_alternative<TelnetDisconnect>(msg)) {
            telnet_msg = std::get<TelnetDisconnect>(msg);
        } else {
            const auto &client_msg = std::get<TelnetClientMessage>(msg);
            if(std::holds_alternative<TelnetMessageData>(client_msg)) {
                TelnetMessageData data_msg = std::get<TelnetMessageData>(client_msg);
                telnet_msg = TelnetMessage{data_msg};
            } else if(std::holds_alternative<TelnetMessageGMCP>(client_msg)) {
                TelnetMessageGMCP gmcp_msg = std::get<TelnetMessageGMCP>(client_msg);
                telnet_msg = TelnetMessage{gmcp_msg.toSubnegotiation()};
            } else if(std::holds_alternative<TelnetMessageMSSP>(client_msg)) {
                TelnetMessageMSSP mssp_msg = std::get<TelnetMessageMSSP>(client_msg);
                telnet_msg = TelnetMessage{mssp_msg.toSubnegotiation()};
            } else {
                LERROR("{} sendToClient received unknown message variant.", *this);
                co_return;
            }
        }
        
        boost::system::error_code ec;
        co_await outgoing_messages_.async_send(ec, std::move(telnet_msg), boost::asio::use_awaitable);
        if(ec) {
            LERROR("{} sendToClient channel error: {}", *this, ec.message());
        }
        co_return;
    }

    std::shared_ptr<TelnetLink> TelnetConnection::make_link() const {
        auto link = std::make_shared<TelnetLink>();
        link->connection_id = conn_.id();
        link->address = conn_.endpoint().address();
        link->hostname = conn_.hostname();
        link->client_data = client_data_;
        link->to_game = to_game_messages_;
        link->to_telnet = to_telnet_messages_;
        return link;
    }

    Channel<std::shared_ptr<TelnetLink>>& link_channel() {
        static Channel<std::shared_ptr<TelnetLink>> channel(volcano::net::context(), 256);
        return channel;
    }

    boost::asio::awaitable<void> handle_linked_telnet(volcano::net::AnyStream&& stream,
        boost::asio::steady_timer::duration negotiation_timeout) {
        auto telnet = std::make_shared<TelnetConnection>(std::move(stream));
        telnet->set_negotiation_timeout(negotiation_timeout);
        co_await telnet->run();
    }
    boost::asio::awaitable<void> TelnetConnection::handleAppData(TelnetMessageData& app_data) {
        append_data_buffer_ += app_data.data;

        if(append_data_buffer_.size() > telnet_limits.max_appdata_buffer) {
            LERROR("{} appdata buffer exceeded limit ({} bytes).", *this, telnet_limits.max_appdata_buffer);
            co_await sendAppData("Input line too long. Disconnecting.\r\n");
            co_await sendToClient(TelnetDisconnect::appdata_overflow);
            boost::system::error_code send_ec;
            co_await to_game_messages_->async_send(send_ec, TelnetDisconnect::appdata_overflow, boost::asio::use_awaitable);
            co_await volcano::net::waitForever(cancellation_signal_);
            co_return;
        }

        auto send_line = [this](std::string line) -> boost::asio::awaitable<void> {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            boost::system::error_code ec;
            co_await to_game_messages_->async_send(ec, TelnetMessageData{std::move(line)}, boost::asio::use_awaitable);
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
            co_await to_game_messages_->async_send(ec, TelnetMessageGMCP{std::move(msg.package), std::move(msg.data)}, boost::asio::use_awaitable);
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
        co_await outgoing_messages_.async_send(ec, TelnetMessage{TelnetMessageData{std::string(app_data)}}, boost::asio::use_awaitable);
        if(ec) {
            LERROR("{} outgoing channel error: {}", *this, ec.message());
        }
        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::sendSubNegotiation(char option, std::string_view sub_data) {
        boost::system::error_code ec;
        co_await outgoing_messages_.async_send(ec, TelnetMessage{TelnetMessageSubnegotiation{option, std::string(sub_data)}}, boost::asio::use_awaitable);
        if(ec) {
            LERROR("{} outgoing channel error: {}", *this, ec.message());
        }
        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::sendNegotiation(char command, char option) {
        boost::system::error_code ec;
        co_await outgoing_messages_.async_send(ec, TelnetMessage{TelnetMessageNegotiation{command, option}}, boost::asio::use_awaitable);
        if(ec) {
            LERROR("{} outgoing channel error: {}", *this, ec.message());
        }
        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::sendCommand(char command) {
        boost::system::error_code ec;
        co_await outgoing_messages_.async_send(ec, TelnetMessage{TelnetMessageCommand{command}}, boost::asio::use_awaitable);
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
                co_await keepalive_timer.async_wait(
                    boost::asio::bind_cancellation_slot(
                        cancellation_signal_.slot(),
                        boost::asio::redirect_error(boost::asio::use_awaitable, timer_ec)));
                if(timer_ec) {
                    if(timer_ec == boost::asio::error::operation_aborted) {
                        co_return;
                    }
                    LERROR("{} keepalive timer error: {}", *this, timer_ec.message());
                    signalShutdown(TelnetShutdownReason::error);
                    co_return;
                }

                if(telnet_mode) {
                    co_await sendCommand(codes::NOP);
                }
            }
        } catch(const boost::system::system_error& e) {
              LERROR("{} keepalive encountered an error: {}", *this, e.what());
              signalShutdown(TelnetShutdownReason::error);
        }

        co_return;
    }

    boost::asio::awaitable<void> TelnetConnection::notifyChangedCapabilities(nlohmann::json& capabilities) {
        boost::system::error_code ec;
        co_await to_game_messages_->async_send(ec, TelnetChangeCapabilities{capabilities}, boost::asio::use_awaitable);
        if(ec) {
            LERROR("{} to_game channel error: {}", *this, ec.message());
        }
        co_return;
    }

}