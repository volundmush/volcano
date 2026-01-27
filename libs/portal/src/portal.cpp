#include "volcano/portal/portal.hpp"

namespace volcano::portal {

    Portal::Portal(PortalConfig config) : config_(std::move(config)) {}

    std::string_view Portal::name() const noexcept {
        return config_.name;
    }

} // namespace volcano::portal
