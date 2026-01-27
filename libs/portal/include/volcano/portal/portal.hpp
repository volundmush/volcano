#pragma once

#include <string>
#include <string_view>

#include "Client.hpp"

namespace volcano::portal {

    struct PortalConfig {
        std::string name{"volcano"};
    };

    class Portal {
    public:
        explicit Portal(PortalConfig config = {});

        [[nodiscard]] std::string_view name() const noexcept;

    private:
        PortalConfig config_{};
    };

} // namespace volcano::portal
