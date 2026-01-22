export module volcano.telnet;

export import :base;
export import :options;
export import :connection;

#if 0
module;

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <fmt/format.h>
#include <zlib.h>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>

export module volcano.telnet;

import volcano.client;
import volcano.log;
import volcano.net;
import volcano.zlib;
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
    class TelnetOption;

    struct TelnetOptionState {
        bool enabled{false};
        bool negotiating{false};
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

namespace {
    std::expected<std::pair<volcano::telnet::TelnetMessage, std::size_t>, std::string>
    parseTelnetMessage(std::string_view data)
    {
        using namespace volcano::telnet;

        if (data.empty()) {
            return std::unexpected("No data to parse");
        }

        auto avail = data.size();

        if (data[0] == codes::IAC) {
            // we're doing an IAC sequence.
            if (avail < 2) {
                return std::unexpected("Incomplete IAC sequence - need at least 2 bytes");
            }

            switch (data[1]) {
            case codes::WILL:
            case codes::WONT:
            case codes::DO:
            case codes::DONT:
            {
                if (avail < 3) {
                    return std::unexpected("Incomplete negotiation sequence - need at least 3 bytes");
                }
                TelnetMessage msg = TelnetMessageNegotiation{data[1], data[2]};
                return std::make_pair(msg, 3);
            }

            case codes::SB:
            {
                // subnegotiation: IAC SB <op> [<data>] IAC SE
                if (avail < 5) {
                    return std::unexpected("Incomplete subnegotiation sequence - need at least 5 bytes");
                }
                auto op = data[2];
                // we know that we start with IAC SB <op>... now we need to scan until we find an unescaped IAC SE
                std::size_t pos = 3;
                while (pos + 1 < avail) {
                    if (data[pos] == codes::IAC) {
                        if (data[pos + 1] == codes::SE) {
                            // end of subnegotiation
                            std::string sub_data;
                            if (pos > 3) {
                                sub_data.reserve(pos - 3);
                                std::size_t i = 3;
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
                        } else if (data[pos + 1] == codes::IAC) {
                            // escaped 255 byte, skip it
                            pos += 2;
                        } else {
                            // something else - just continue
                            pos += 1;
                        }
                    } else {
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
        } else {
            // regular data
            std::size_t pos = data.find(codes::IAC);
            if (pos == std::string_view::npos) {
                pos = data.size();
            }
            TelnetMessage msg = TelnetMessageData{std::string(data.substr(0, pos))};
            return std::make_pair(msg, pos);
        }
    }

    void append_iac_escaped(std::string& out, std::string_view data) {
        for (char ch : data) {
            out.push_back(ch);
            if (static_cast<unsigned char>(ch) == static_cast<unsigned char>(volcano::telnet::codes::IAC)) {
                out.push_back(volcano::telnet::codes::IAC);
            }
        }
    }

    void append_subnegotiation(std::string& out, char option, std::string_view data) {
        using namespace volcano::telnet;
        out.push_back(codes::IAC);
        out.push_back(codes::SB);
        out.push_back(option);
        append_iac_escaped(out, data);
        out.push_back(codes::IAC);
        out.push_back(codes::SE);
    }

    std::string encodeTelnetMessage(const volcano::telnet::TelnetMessage& msg) {
        using namespace volcano::telnet;
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

    std::string to_upper_copy(std::string_view input) {
        std::string out(input);
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return out;
    }
}

namespace volcano::telnet {
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

    std::pair<bool, bool> CHARSETOption::getLocalSupportInfo() {
        return {true, true};
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

    std::pair<bool, bool> MTTSOption::getRemoteSupportInfo() {
        return {true, true};
    }

    boost::asio::awaitable<void> MTTSOption::at_remote_enable() {
        client_data().mtts = true;
        nlohmann::json capabilities;
        capabilities["mtts"] = true;
        co_await notifyChangedCapabilities(capabilities);
        co_await request();
        co_return;
    }

    boost::asio::awaitable<void> MTTSOption::request() {
        number_requests_ += 1;
        std::string data;
        data.push_back(static_cast<char>(0x01));
        co_await send_subnegotiate(data);
        co_return;
    }

    boost::asio::awaitable<void> MTTSOption::handle_name(std::string_view data) {
        nlohmann::json out;

        std::string payload(data);
        auto space_pos = payload.find(' ');
        std::string client_name;
        std::string client_version;

        if (space_pos != std::string::npos) {
            client_name = payload.substr(0, space_pos);
            client_version = payload.substr(space_pos + 1);
        } else {
            client_name = payload;
        }

        if (!client_name.empty()) {
            client_data().client_name = client_name;
            out["client_name"] = client_name;
        }

        if (!client_version.empty()) {
            client_data().client_version = client_version;
            out["client_version"] = client_version;
        }

        int max_color = 1;
        std::string upper_name = to_upper_copy(client_name);

        if (upper_name == "ATLANTIS" || upper_name == "CMUD" || upper_name == "KILDCLIENT" ||
            upper_name == "MUDLET" || upper_name == "MUSHCLIENT" || upper_name == "PUTTY" ||
            upper_name == "POTATO" || upper_name == "TINYFUGUE") {
            max_color = std::max(max_color, 2);
        } else if (upper_name == "BEIP") {
            max_color = std::max(max_color, 3);
        } else if (upper_name == "MUDLET") {
            if (!client_version.empty() && client_version.rfind("1.1", 0) == 0) {
                max_color = std::max(max_color, 2);
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

#endif
