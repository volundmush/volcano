#include "volcano/telnet/Base.hpp"

namespace volcano::telnet {

	TelnetLimits telnet_limits;

	TelnetMessageSubnegotiation TelnetMessageGMCP::toSubnegotiation() const {
		TelnetMessageSubnegotiation out;
		out.option = codes::GMCP;
		out.data = package;
		if (!data.is_null()) {
			out.data += " ";
			out.data += data.dump();
		}
		return out;
	}

	TelnetMessageSubnegotiation TelnetMessageMSSP::toSubnegotiation() const {
		TelnetMessageSubnegotiation out;
		out.option = codes::MSSP;
		std::string payload;
		for (const auto& [key, value] : variables) {
			payload.push_back(static_cast<char>(1));
			payload += key;
			payload.push_back(static_cast<char>(2));
			payload += value;
		}
		out.data = std::move(payload);
		return out;
	}
}