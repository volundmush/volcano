#include "volcano/mud/CircleAnsi.hpp"

#define ANSISTART "\x1B["
#define ANSISEP ';'
#define ANSISEPSTR ";"
#define ANSIEND 'm'
#define ANSIENDSTR "m"

/* Attributes */
#define AA_NORMAL "0"
#define AA_BOLD "1"
#define AA_UNDERLINE "4"
#define AA_BLINK "5"
#define AA_REVERSE "7"
#define AA_INVIS "8"
/* Foreground colors */
#define AF_BLACK "30"
#define AF_RED "31"
#define AF_GREEN "32"
#define AF_YELLOW "33"
#define AF_BLUE "34"
#define AF_MAGENTA "35"
#define AF_CYAN "36"
#define AF_WHITE "37"
/* Background colors */
#define AB_BLACK "40"
#define AB_RED "41"
#define AB_GREEN "42"
#define AB_YELLOW "43"
#define AB_BLUE "44"
#define AB_MAGENTA "45"
#define AB_CYAN "46"
#define AB_WHITE "47"

namespace volcano::mud::circle
{

    namespace
    {
        const char RANDOM_COLORS[] = "bgcrmywBGCRMWY";

        const char *ANSI[] = {
            "@",
            AA_NORMAL,
            AA_NORMAL ANSISEPSTR AF_BLACK,
            AA_NORMAL ANSISEPSTR AF_BLUE,
            AA_NORMAL ANSISEPSTR AF_GREEN,
            AA_NORMAL ANSISEPSTR AF_CYAN,
            AA_NORMAL ANSISEPSTR AF_RED,
            AA_NORMAL ANSISEPSTR AF_MAGENTA,
            AA_NORMAL ANSISEPSTR AF_YELLOW,
            AA_NORMAL ANSISEPSTR AF_WHITE,
            AA_BOLD ANSISEPSTR AF_BLACK,
            AA_BOLD ANSISEPSTR AF_BLUE,
            AA_BOLD ANSISEPSTR AF_GREEN,
            AA_BOLD ANSISEPSTR AF_CYAN,
            AA_BOLD ANSISEPSTR AF_RED,
            AA_BOLD ANSISEPSTR AF_MAGENTA,
            AA_BOLD ANSISEPSTR AF_YELLOW,
            AA_BOLD ANSISEPSTR AF_WHITE,
            AB_BLACK,
            AB_BLUE,
            AB_GREEN,
            AB_CYAN,
            AB_RED,
            AB_MAGENTA,
            AB_YELLOW,
            AB_WHITE,
            AA_BLINK,
            AA_UNDERLINE,
            AA_BOLD,
            AA_REVERSE,
            "!"};

        const char CCODE[] = "@ndbgcrmywDBGCRMYW01234567luoex!";

        constexpr int NUM_COLOR = 16;

        const char *default_color_choices[NUM_COLOR + 1] = {
            /* COLOR_NORMAL */ AA_NORMAL,
            /* COLOR_ROOMNAME */ AA_NORMAL ANSISEPSTR AF_CYAN,
            /* COLOR_ROOMOBJS */ AA_NORMAL ANSISEPSTR AF_GREEN,
            /* COLOR_ROOMPEOPLE */ AA_NORMAL ANSISEPSTR AF_YELLOW,
            /* COLOR_HITYOU */ AA_NORMAL ANSISEPSTR AF_RED,
            /* COLOR_YOUHIT */ AA_NORMAL ANSISEPSTR AF_GREEN,
            /* COLOR_OTHERHIT */ AA_NORMAL ANSISEPSTR AF_YELLOW,
            /* COLOR_CRITICAL */ AA_BOLD ANSISEPSTR AF_YELLOW,
            /* COLOR_HOLLER */ AA_BOLD ANSISEPSTR AF_YELLOW,
            /* COLOR_SHOUT */ AA_BOLD ANSISEPSTR AF_YELLOW,
            /* COLOR_GOSSIP */ AA_NORMAL ANSISEPSTR AF_YELLOW,
            /* COLOR_AUCTION */ AA_NORMAL ANSISEPSTR AF_CYAN,
            /* COLOR_CONGRAT */ AA_NORMAL ANSISEPSTR AF_GREEN,
            /* COLOR_TELL */ AA_NORMAL ANSISEPSTR AF_RED,
            /* COLOR_YOUSAY */ AA_NORMAL ANSISEPSTR AF_CYAN,
            /* COLOR_ROOMSAY */ AA_NORMAL ANSISEPSTR AF_WHITE,
            nullptr};
    }

    int count_color_chars(std::string_view string)
    {
        int num = 0;

        for (size_t i = 0; i < string.size(); i++)
        {
            while (string[i] == '@')
            {
                if (string[i + 1] == '@')
                {
                    num++;
                }
                else if (string[i + 1] == '[')
                {
                    num += 4;
                }
                else
                {
                    num += 2;
                }
                i += 2;
            }
        }
        return num;
    }

    // A C++ version of proc_color from comm.c. it returns the colored string.
    std::string processColors(std::string_view txt, int parse, char **choices)
    {
        const char *color_char;
        const char *replacement = nullptr;
        int i, temp_color;

        if (txt.empty() || txt.find('@') == std::string_view::npos) /* skip out if no color codes     */
            return std::string(txt);

        std::string out;
        for (size_t pos = 0; pos < txt.size();)
        {
            /* no color code - just copy */
            if (txt[pos] != '@')
            {
                out.push_back(txt[pos++]);
                continue;
            }

            /* if we get here we have a color code */

            pos++; /* now points to the code */
            if (pos >= txt.size())
            {
                out.push_back('@');
                break;
            }

            char code = txt[pos];

            /* look for a random color code picks a random number between 1 and 14 */
            if (code == 'x')
            {
                temp_color = (rand() % 14);
                code = RANDOM_COLORS[temp_color];
            }

            if (!parse)
            { /* not parsing, just skip the code, unless it's @@ */
                if (code == '@')
                {
                    out.push_back('@');
                }
                if (code == '[')
                { /* Multi-character code */
                    pos++;
                    while (pos < txt.size() && isdigit(static_cast<unsigned char>(txt[pos])))
                        pos++;
                    if (pos >= txt.size())
                        pos = txt.size() - 1;
                }
                pos++; /* skip to next (non-colorcode) char */
                continue;
            }

            /* parse the color code */
            if (code == '[')
            { /* User configurable color */
                pos++;
                if (pos < txt.size())
                {
                    i = atoi(txt.data() + pos);
                    if (i < 0 || i >= NUM_COLOR)
                        i = COLOR_NORMAL;
                    replacement = default_color_choices[i];
                    if (choices && choices[i])
                        replacement = choices[i];
                    while (pos < txt.size() && isdigit(static_cast<unsigned char>(txt[pos])))
                        pos++;
                    if (pos >= txt.size())
                        pos = txt.size() - 1;
                }
            }
            else if (code == 'n')
            {
                replacement = default_color_choices[COLOR_NORMAL];
                if (choices && choices[COLOR_NORMAL])
                    replacement = choices[COLOR_NORMAL];
            }
            else
            {
                for (i = 0; CCODE[i] != '!'; i++)
                { /* do we find it ? */
                    if (code == CCODE[i])
                    { /* if so :*/
                        replacement = ANSI[i];
                        break;
                    }
                }
            }
            if (replacement)
            {
                if (isdigit(replacement[0]))
                    for (color_char = ANSISTART; *color_char;)
                        out.push_back(*color_char++);
                for (color_char = replacement; *color_char;)
                    out.push_back(*color_char++);
                if (isdigit(replacement[0]))
                    out.push_back(ANSIEND);
                replacement = nullptr;
            }
            /* If we couldn't find any correct color code, or we found it and
             * substituted above, let's just process the next character.
             * - Welcor
             */
            pos++;

        } /* for loop */

        return out;
    }

    size_t countColors(std::string_view txt)
    {
        auto stripped = processColors(txt, false, nullptr);
        return txt.size() - stripped.size();
    }

    bool isColorChar(char c)
    {
        switch (c)
        {
        case 'n':
        case 'b':
        case 'B':
        case 'g':
        case 'G':
        case 'm':
        case 'M':
        case 'r':
        case 'R':
        case 'y':
        case 'Y':
        case 'w':
        case 'W':
        case 'k':
        case 'K':
        case '0':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case 'l':
        case 'u':
        case 'o':
        case 'e':
            // case 'x':
            return true;
        default:
            return false;
        }
    }
}