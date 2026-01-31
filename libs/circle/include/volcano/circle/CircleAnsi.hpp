#pragma once
#include <string>
#include <string_view>
#include "volcano/ansi/Text.hpp"

namespace volcano::circle
{

    constexpr int COLOR_NORMAL = 0;
    constexpr int COLOR_ROOMNAME = 1;
    constexpr int COLOR_ROOMOBJS = 2;
    constexpr int COLOR_ROOMPEOPLE = 3;
    constexpr int COLOR_HITYOU = 4;
    constexpr int COLOR_YOUHIT = 5;
    constexpr int COLOR_OTHERHIT = 6;
    constexpr int COLOR_CRITICAL = 7;
    constexpr int COLOR_HOLLER = 8;
    constexpr int COLOR_SHOUT = 9;
    constexpr int COLOR_GOSSIP = 10;
    constexpr int COLOR_AUCTION = 11;
    constexpr int COLOR_CONGRAT = 12;
    constexpr int COLOR_TELL = 13;
    constexpr int COLOR_YOUSAY = 14;
    constexpr int COLOR_ROOMSAY = 15;
    
    volcano::ansi::Text toText(std::string_view txt);
    std::string processColors(std::string_view txt, int parse, char **choices);
    size_t countColors(std::string_view txt);

    bool isColorChar(char c);
}
