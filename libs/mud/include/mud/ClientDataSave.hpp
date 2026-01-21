#pragma once

#include "mud/ClientData.hpp"
#include <nlohmann/json_fwd.hpp>

namespace vol::mud {
    void to_json(nlohmann::json& j, const ClientData& data);
    void from_json(const nlohmann::json& j, ClientData& data);
}