#include "telnet/telnet.hpp"
#include "logging/Log.hpp"
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/steady_timer.hpp>
#include <zlib/zlib.hpp>

#include <cstddef>
#include <cstring>
#include <span>

namespace vol::telnet {

    TelnetConnection::TelnetConnection(vol::net::AnyStream connection)
        : conn_(std::move(connection)), 
        outgoing_messages_(conn_.get_executor(), 100),
        to_game_messages_(conn_.get_executor(), 100) {
            client_data_.connection_id = conn_.id();
            client_data_.tls = conn_.is_tls();
            client_data_.client_address = conn_.endpoint().address().to_string();
            client_data_.client_hostname = conn_.hostname();
            client_data_.client_protocol = "telnet";
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
        vol::zlib::InflateStream inflater;
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
                        changed_capabilities_["mccp3_enabled"] = true;
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
        vol::zlib::DeflateStream deflater(Z_BEST_COMPRESSION);
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
                        }, vol::zlib::FlushMode::sync);
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
                        changed_capabilities_["mccp2_enabled"] = true;
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

    boost::asio::awaitable<void> TelnetConnection::processAppData(TelnetMessageData& app_data) {
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

    boost::asio::awaitable<void> TelnetConnection::processData(TelnetMessage& data) {
        if(std::holds_alternative<TelnetMessageData>(data)) {
            TelnetMessageData& msg = std::get<TelnetMessageData>(data);
            co_await processAppData(msg);
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

    boost::asio::awaitable<void> TelnetConnection::notifyChangedCapabilities(nlohmann::json& capabilities) {
        boost::system::error_code ec;
        co_await to_game_messages_.async_send(ec, TelnetChangeCapabilities{capabilities}, boost::asio::use_awaitable);
        if(ec) {
            LERROR("{} to_game channel error: {}", *this, ec.message());
        }
        co_return;
    }

}