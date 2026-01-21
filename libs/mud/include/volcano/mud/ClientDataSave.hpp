#pragma once

#include "ClientData.hpp"
#include <nlohmann/json_fwd.hpp>

namespace volcano::mud {
    void to_json(nlohmann::json& j, const ClientData& data);
    void from_json(const nlohmann::json& j, ClientData& data);
}