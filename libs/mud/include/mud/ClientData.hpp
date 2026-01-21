#pragma once
#include <cstdint>
#include <string>

namespace vol::mud
{
    struct ClientData
    {
        std::string client_address = "UNKNOWN";
        std::string client_hostname = "UNKNOWN";
        std::string client_protocol = "UNKNOWN";
        std::string client_name = "UNKNOWN";
        std::string client_version = "UNKNOWN";
        std::string encoding = "ascii";
        bool tls = false;
        uint8_t color = 0;
        uint16_t width = 78;
        uint16_t height = 24;
        bool mccp2 = false;
        bool mccp2_enabled = false;
        bool mccp3 = false;
        bool mccp3_enabled = false;
        bool gmcp = false;
        bool mtts = false;
        bool naws = false;
        bool charset = false;
        bool mnes = false;
        bool linemode = false;
        bool sga = false;
        bool force_endline = false;
        bool screen_reader = false;
        bool mouse_tracking = false;
        bool vt100 = false;
        bool osc_color_palette = false;
        bool proxy = false;
        bool tls_support = false;
    };
}