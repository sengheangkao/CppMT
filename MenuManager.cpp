#include "MenuManager.h"
#include "DataManager.h"
#include "tabulate.hpp"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <limits>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include <thread>
#include <atomic>
#include <chrono>
#include <utility>
#include <vector>
#include <clocale>
#include <cwchar>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define RESET   "[0m"
#define BOLD    "[1m"
#define DIM     "[2m"
// ── Single Ocean Blue Theme ──────────────────────────────
#define B1      "[38;2;3;4;94m"      // #03045e darkest navy  — deep accents
#define B2      "[38;2;0;119;182m"   // #0077b6 ocean blue    — borders, headers
#define B3      "[38;2;0;180;216m"   // #00b4d8 sky blue      — titles, prompts
#define B4      "[38;2;144;224;239m" // #90e0ef light blue    — menu text
#define B5      "[38;2;202;240;248m" // #caf0f8 pale blue     — dim info
#define GREEN   "[38;2;88;187;67m"   // fresh green — available/success
#define LGREEN  "[38;2;193;255;28m"  // lime green  — highlights
#define YELLOW  "[38;2;148;0;211m"   // violet      — warnings/TODAY
#define RED     "[38;2;255;100;100m" // soft red    — errors/taken
// Aliases for backward compat
#define PINK    B3
#define LPINK   B4
#define DPINK   B2
#define MPINK   B3
#define SPINK   B4
#define CYAN    B3
#define WHITE   "[97m"
#define MBLUE   B2
#define LBLUE   B4

// ── Center text — strips ANSI codes to get true visible length ──────────────
static int visLen(const std::string& s) {
    static const bool localeReady = []() {
        std::setlocale(LC_CTYPE, "");
        return true;
    }();
    (void)localeReady;

    int len = 0;
    std::mbstate_t state {};
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == '\033') {
            while (i < s.size() && s[i] != 'm') i++;
            if (i < s.size()) i++;
            state = std::mbstate_t {};
            continue;
        }

        if (c < 0x20) {
            i++;
            continue;
        }

        wchar_t wc = 0;
        size_t used = std::mbrtowc(&wc, s.c_str() + i, s.size() - i, &state);
        if (used == static_cast<size_t>(-1) || used == static_cast<size_t>(-2)) {
            state = std::mbstate_t {};
            len++;
            i++;
            continue;
        }
        if (used == 0) {
            i++;
            continue;
        }

        int width = ::wcwidth(wc);
        if (width > 0) {
            len += width;
        } else {
            state = std::mbstate_t {};
        }
        i += used;
    }
    return len;
}

static int terminalWidth() {
    struct winsize w {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return static_cast<int>(w.ws_col);
    }
    return 100;
}

static std::string indentFor(int visibleWidth) {
    return std::string(std::max(0, (terminalWidth() - visibleWidth) / 2), ' ');
}

static std::string centerIn(const std::string& text, int width) {
    int pad = width - visLen(text);
    int left = std::max(0, pad / 2);
    int right = std::max(0, pad - left);
    return std::string(left, ' ') + text + std::string(right, ' ');
}

static std::string repeat(const std::string& text, int count) {
    std::string out;
    for (int i = 0; i < count; ++i) out += text;
    return out;
}

static std::string padRightVisible(const std::string& text, int width) {
    return text + std::string(std::max(0, width - visLen(text)), ' ');
}

static std::string ltrimPrompt(std::string prompt) {
    while (!prompt.empty() && prompt.front() == ' ') prompt.erase(prompt.begin());
    return prompt;
}

static void printInputPrompt(const std::string& prompt) {
    size_t lastNewline = prompt.find_last_of('\n');
    std::string prefix;
    std::string line = prompt;
    if (lastNewline != std::string::npos) {
        prefix = prompt.substr(0, lastNewline + 1);
        line = prompt.substr(lastNewline + 1);
    }
    line = ltrimPrompt(line);
    std::cout << prefix << indentFor(visLen(line)) << line;
}

static std::string center(const std::string& text, int width=0) {
    if (width <= 0) width = terminalWidth();
    int len = visLen(text);
    if (len >= width) return text;
    int pad = (width - len) / 2;
    return std::string(pad, ' ') + text;
}
static void printCentered(const std::string& text, int width=0) {
    std::cout << center(text, width) << "\n";
}

static std::vector<std::string> glyphFor(char ch) {
    switch (std::toupper(static_cast<unsigned char>(ch))) {
        case 'A': return {"  ###  ", " #   # ", "#     #", "#######", "#     #", "#     #", "#     #"};
        case 'B': return {"###### ", "#     #", "#     #", "###### ", "#     #", "#     #", "###### "};
        case 'C': return {" ##### ", "#     #", "#      ", "#      ", "#      ", "#     #", " ##### "};
        case 'D': return {"###### ", "#     #", "#     #", "#     #", "#     #", "#     #", "###### "};
        case 'E': return {"#######", "#      ", "#      ", "#####  ", "#      ", "#      ", "#######"};
        case 'F': return {"#######", "#      ", "#      ", "#####  ", "#      ", "#      ", "#      "};
        case 'G': return {" ##### ", "#     #", "#      ", "#  ####", "#     #", "#     #", " ##### "};
        case 'H': return {"#     #", "#     #", "#     #", "#######", "#     #", "#     #", "#     #"};
        case 'I': return {"#######", "   #   ", "   #   ", "   #   ", "   #   ", "   #   ", "#######"};
        case 'J': return {"#######", "     # ", "     # ", "     # ", "#    # ", "#    # ", " ####  "};
        case 'K': return {"#    # ", "#   #  ", "#  #   ", "###    ", "#  #   ", "#   #  ", "#    # "};
        case 'L': return {"#      ", "#      ", "#      ", "#      ", "#      ", "#      ", "#######"};
        case 'M': return {"#     #", "##   ##", "# # # #", "#  #  #", "#     #", "#     #", "#     #"};
        case 'N': return {"#     #", "##    #", "# #   #", "#  #  #", "#   # #", "#    ##", "#     #"};
        case 'O': return {" ##### ", "#     #", "#     #", "#     #", "#     #", "#     #", " ##### "};
        case 'P': return {"###### ", "#     #", "#     #", "###### ", "#      ", "#      ", "#      "};
        case 'Q': return {" ##### ", "#     #", "#     #", "#     #", "#   # #", "#    # ", " #### #"};
        case 'R': return {"###### ", "#     #", "#     #", "###### ", "#   #  ", "#    # ", "#     #"};
        case 'S': return {" ######", "#      ", "#      ", " ##### ", "      #", "      #", "###### "};
        case 'T': return {"#######", "   #   ", "   #   ", "   #   ", "   #   ", "   #   ", "   #   "};
        case 'U': return {"#     #", "#     #", "#     #", "#     #", "#     #", "#     #", " ##### "};
        case 'V': return {"#     #", "#     #", "#     #", "#     #", " #   # ", "  # #  ", "   #   "};
        case 'W': return {"#     #", "#     #", "#     #", "#  #  #", "# # # #", "##   ##", "#     #"};
        case 'X': return {"#     #", " #   # ", "  # #  ", "   #   ", "  # #  ", " #   # ", "#     #"};
        case 'Y': return {"#     #", " #   # ", "  # #  ", "   #   ", "   #   ", "   #   ", "   #   "};
        case 'Z': return {"#######", "     # ", "    #  ", "   #   ", "  #    ", " #     ", "#######"};
        case '0': return {" ##### ", "#    ##", "#   # #", "#  #  #", "# #   #", "##    #", " ##### "};
        case '1': return {"   #   ", "  ##   ", " # #   ", "   #   ", "   #   ", "   #   ", " ##### "};
        case '2': return {" ##### ", "#     #", "      #", "   ### ", "  #    ", " #     ", "#######"};
        case '3': return {" ##### ", "#     #", "      #", "  #### ", "      #", "#     #", " ##### "};
        case '4': return {"#    # ", "#    # ", "#    # ", "#######", "     # ", "     # ", "     # "};
        case '5': return {"#######", "#      ", "#      ", "###### ", "      #", "#     #", " ##### "};
        case '6': return {" ##### ", "#     #", "#      ", "###### ", "#     #", "#     #", " ##### "};
        case '7': return {"#######", "     # ", "    #  ", "   #   ", "  #    ", " #     ", "#      "};
        case '8': return {" ##### ", "#     #", "#     #", " ##### ", "#     #", "#     #", " ##### "};
        case '9': return {" ##### ", "#     #", "#     #", " ######", "      #", "#     #", " ##### "};
        default:  return {"    ", "    ", "    ", "    ", "    ", "    ", "    "};
    }
}

static std::vector<std::string> blockTextLines(const std::string& text) {
    std::vector<std::string> lines(7);
    for (char ch : text) {
        auto glyph = glyphFor(ch);
        for (size_t i = 0; i < lines.size(); ++i) {
            lines[i] += glyph[i] + std::string(" ");
        }
    }
    return lines;
}

static std::vector<std::string> shadowTextLines(const std::string& text,
                                                const std::string& color = GREEN) {
    std::vector<std::string> raw = blockTextLines(text);
    size_t width = 0;
    for (const auto& line : raw) width = std::max(width, line.size());
    for (auto& line : raw) line += std::string(width - line.size(), ' ');

    std::vector<std::string> output;
    for (size_t y = 0; y < raw.size() + 1; ++y) {
        std::string line;
        for (size_t x = 0; x < width + 2; ++x) {
            bool fg = y < raw.size() && x < raw[y].size() && raw[y][x] != ' ';
            bool sh = y > 0 && x >= 2 && (y - 1) < raw.size()
                      && (x - 2) < raw[y - 1].size() && raw[y - 1][x - 2] != ' ';
            if (fg) line += color + BOLD + "█" + RESET;
            else if (sh) line += std::string(B3) + DIM + "░" + RESET;
            else line += " ";
        }
        output.push_back(line);
    }
    return output;
}

static std::vector<std::string> theaterBannerLines(const std::string& title) {
    const int frameW = 68;
    const int contentW = frameW - 2;
    auto top = [&]() {
        return std::string(B3) + "╭" + repeat("─", contentW) + "╮" + RESET;
    };
    auto bottom = [&]() {
        return std::string(B3) + "╰" + repeat("─", contentW) + "╯" + RESET;
    };
    auto line = [&](const std::string& content) {
        return std::string(B3) + "│" + RESET + centerIn(content, contentW)
             + std::string(B3) + "│" + RESET;
    };
    auto splitLine = [&](const std::string& left, const std::string& middle, const std::string& right) {
        int spaces = contentW - visLen(left) - visLen(middle) - visLen(right);
        int leftSpaces = std::max(0, spaces / 2);
        int rightSpaces = std::max(0, spaces - leftSpaces);
        return std::string(B3) + "│" + RESET + left + std::string(leftSpaces, ' ')
             + middle + std::string(rightSpaces, ' ') + right
             + std::string(B3) + "│" + RESET;
    };

    std::vector<std::string> lines;
    lines.push_back(top());
    lines.push_back(line(std::string(B3) + "╭╮ ╭╮ ╭╮ ╭╮ ╭╮ ╭╮ ╭╮ ╭╮ ╭╮ ╭╮" + RESET));
    lines.push_back(splitLine(std::string(B3) + "╲╲╲╲╲" + RESET, "", std::string(B3) + "╱╱╱╱╱" + RESET));
    lines.push_back(splitLine(std::string(B3) + " ╲╲╲╲" + RESET, "", std::string(B3) + "╱╱╱╱ " + RESET));
    for (const auto& artLine : shadowTextLines(title, B3)) {
        lines.push_back(line(artLine));
    }
    lines.push_back(line(std::string(B4) + BOLD + "MOVIE TICKET MANAGEMENT SYSTEM" + RESET));
    lines.push_back(line(""));
    lines.push_back(line(std::string(B3) + "╭──╮ ╭──╮ ╭──╮ ╭──╮ ╭──╮ ╭──╮ ╭──╮" + RESET));
    lines.push_back(line(std::string(B3) + "╰──╯ ╰──╯ ╰──╯ ╰──╯ ╰──╯ ╰──╯ ╰──╯" + RESET));
    lines.push_back(line(std::string(B3) + "★  WHERE MOVIES COME ALIVE  ★" + RESET));
    lines.push_back(bottom());
    return lines;
}

static std::vector<std::string> clapperBannerLines(const std::string& title) {
    const int frameW = 68;
    const int contentW = frameW - 2;
    auto line = [&](const std::string& content) {
        return centerIn(content, contentW);
    };
    auto splitLine = [&](const std::string& left, const std::string& right) {
        int spaces = contentW - visLen(left) - visLen(right);
        return left + std::string(std::max(0, spaces), ' ') + right;
    };

    std::vector<std::string> lines;
    lines.push_back(line(std::string(B3) + "╭──────────────╮" + RESET));
    lines.push_back(line(std::string(B3) + "│ ▰ ▱ ▰ ▱ ▰ ▱ │" + RESET));
    lines.push_back(line(std::string(B3) + "├──────────────┤" + RESET));
    lines.push_back(line(std::string(B3) + "│              │" + RESET));
    lines.push_back(line(std::string(B3) + "╰──────────────╯" + RESET));
    lines.push_back(splitLine(std::string("\033[38;2;148;0;211m") + "──────────" + RESET,
                              std::string("\033[38;2;148;0;211m") + "──────────" + RESET));
    for (const auto& artLine : shadowTextLines(title, std::string("\033[38;2;148;0;211m"))) {
        lines.push_back(line(artLine));
    }
    lines.push_back(line(std::string(B5) + BOLD + "MOVIE TICKET MANAGEMENT SYSTEM" + RESET));
    lines.push_back(line(""));
    lines.push_back(splitLine(std::string("\033[38;2;148;0;211m") + "╭───╮" + RESET,
                              std::string("\033[38;2;148;0;211m") + "╭───╮" + RESET));
    lines.push_back(line(std::string("\033[38;2;148;0;211m") + "🎟  YOUR SEAT, YOUR STORY  🎟" + RESET));
    return lines;
}

static std::vector<std::string> ticketBannerLines(const std::string& title) {
    const int frameW = 68;
    const int contentW = frameW - 2;
    const std::string SKY = B3;
    auto line = [&](const std::string& content) {
        return SKY + "│" + RESET + centerIn(content, contentW)
             + SKY + "│" + RESET;
    };

    std::vector<std::string> lines;
    lines.push_back(SKY + BOLD + "═══════ ☆  ☆  ☆ ═════════════════════════ ☆  ☆  ☆ ═══════" + RESET);
    lines.push_back(SKY + "╭" + repeat("─", 18) + repeat("─", contentW - 36) + repeat("─", 18) + "╮" + RESET);
    lines.push_back(line(""));
    for (const auto& artLine : shadowTextLines(title, SKY)) {
        lines.push_back(line(artLine));
    }
    lines.push_back(line(""));
    lines.push_back(line(std::string(B5) + BOLD + "MOVIE TICKET SYSTEM" + RESET));
    lines.push_back(line(std::string(SKY) + "BOOK  |  WATCH  |  ENJOY" + RESET));
    lines.push_back(line(""));
    lines.push_back(SKY + "╰" + repeat("─", 22) + "╮" + repeat("─", 20) + "╭" + repeat("─", 22) + "╯" + RESET);
    lines.push_back(SKY + BOLD + "═══════ ☆  ☆  ☆ ═════════════════════════ ☆  ☆  ☆ ═══════" + RESET);
    return lines;
}

static std::vector<std::string> goodbyeMarqueeLines(const std::string& title) {
    const int frameW = 68;
    const int contentW = frameW - 2;
    const std::string HOT_PINK = "\033[38;2;255;79;172m";
    auto line = [&](const std::string& content) {
        return HOT_PINK + "│" + RESET + centerIn(content, contentW)
             + HOT_PINK + "│" + RESET;
    };

    std::vector<std::string> lines;
    lines.push_back(centerIn(HOT_PINK + "╭───────◯───────╮" + RESET, frameW));
    lines.push_back(centerIn(HOT_PINK + "│  ◯   ◯   ◯  │" + RESET, frameW));
    lines.push_back(HOT_PINK + "╭" + repeat("─", contentW) + "╮" + RESET);
    lines.push_back(line(""));
    for (const auto& artLine : shadowTextLines(title, HOT_PINK)) {
        lines.push_back(line(artLine));
    }
    lines.push_back(line(HOT_PINK + "-- MOVIE TICKET SYSTEM --" + RESET));
    lines.push_back(line(""));
    lines.push_back(line(HOT_PINK + "RELAX | ENJOY | REPEAT" + RESET));
    lines.push_back(line(HOT_PINK + "☆ ☆ ☆ ☆ ☆" + RESET));
    lines.push_back(HOT_PINK + "╰" + repeat("─", contentW) + "╯" + RESET);
    return lines;
}

static void printBlockText(const std::string& text, const std::string& color = GREEN) {
    for (const auto& line : shadowTextLines(text, color)) {
        printCentered(line);
    }
}

static int printOptionBox(const std::string& title,
                          const std::vector<std::pair<std::string, std::string>>& rows,
                          int selectedIndex = -1,
                          const std::string& choiceLine = "") {
    static const char* ROW_COLORS[] = {
        "\033[38;2;199;21;133m", "\033[38;2;128;0;128m",
        GREEN, "\033[38;2;220;20;60m", "\033[38;2;0;92;255m",
        "\033[38;2;0;38;120m", B3, "\033[38;2;148;0;211m",
        "\033[38;2;176;48;96m"
    };
    constexpr int noW = 6;
    constexpr int optW = 48;
    const int fullW = noW + optW + 3;
    const int titleW = noW + 1 + optW;
    auto border = [&](const std::string& left, const std::string& mid, const std::string& right) {
        std::cout << B2 << indentFor(fullW)
                  << left << repeat("─", noW) << mid << repeat("─", optW) << right
                  << RESET << "\n";
    };
    auto spanBorder = [&](const std::string& left, const std::string& right) {
        std::cout << B2 << indentFor(fullW)
                  << left << repeat("─", titleW) << right
                  << RESET << "\n";
    };
    auto titleRow = [&]() {
        std::cout << B2 << indentFor(fullW) << "│" << RESET
                  << B3 << BOLD << centerIn(title, titleW) << RESET
                  << B2 << "│" << RESET << "\n";
    };
    auto headerRow = [&]() {
        std::cout << B2 << indentFor(fullW) << "│" << RESET
                  << B3 << BOLD << centerIn("No", noW) << RESET
                  << B2 << "│" << RESET
                  << B3 << BOLD << " " << padRightVisible("Option", optW - 1) << RESET
                  << B2 << "│" << RESET << "\n";
    };
    auto choiceRow = [&]() {
        std::cout << B2 << indentFor(fullW) << "│" << RESET
                  << centerIn(choiceLine, titleW)
                  << B2 << "│" << RESET << "\n";
    };
    auto optionRow = [&](const std::string& number, const std::string& label, size_t index) {
        const char* color = ROW_COLORS[index % 9];
        bool selected = static_cast<int>(index) == selectedIndex;
        std::string display = selected ? ("➤ " + label) : (selectedIndex >= 0 ? "  " + label : label);
        std::cout << B2 << indentFor(fullW) << "│" << RESET
                  << B3 << BOLD << centerIn(number, noW) << RESET
                  << B2 << "│" << RESET
                  << color << BOLD << " " << padRightVisible(display, optW - 1) << RESET
                  << B2 << "│" << RESET << "\n";
    };

    int lineCount = 0;
    std::cout << "\n";
    lineCount++;
    spanBorder("┌", "┐");
    lineCount++;
    titleRow();
    lineCount++;
    border("├", "┬", "┤");
    lineCount++;
    headerRow();
    lineCount++;
    border("├", "┼", "┤");
    lineCount++;
    for (size_t i = 0; i < rows.size(); ++i) {
        optionRow(rows[i].first, rows[i].second, i);
        lineCount++;
        if (i + 1 == rows.size() && choiceLine.empty()) {
            border("└", "┴", "┘");
        } else if (i + 1 == rows.size()) {
            border("├", "┴", "┤");
        } else {
            border("├", "┼", "┤");
        }
        lineCount++;
    }
    if (!choiceLine.empty()) {
        choiceRow();
        lineCount++;
        spanBorder("└", "┘");
        lineCount++;
    }
    std::cout << "\n";
    lineCount++;
    return lineCount;
}

static int optionIndexFor(const std::vector<std::pair<std::string, std::string>>& rows, int number) {
    const std::string want = std::to_string(number);
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].first == want) return static_cast<int>(i);
    }
    return -1;
}

static bool rowNumberExists(const std::vector<std::pair<std::string, std::string>>& rows, int number) {
    return optionIndexFor(rows, number) >= 0;
}

static int chooseOptionBox(const std::string& title,
                           const std::vector<std::pair<std::string, std::string>>& rows,
                           const std::string& prompt,
                           int min,
                           int max) {
    if (rows.empty()) return min;

    auto choiceLine = [&](int selected, const std::string& typed) {
        std::string cleanPrompt = ltrimPrompt(prompt);
        std::string value = typed.empty() ? std::to_string(selected) : typed;
        return std::string(B3) + "↑/↓" + RESET + " Select   " +
               std::string(B3) + "Enter" + RESET + " OK   " +
               B3 + std::string(">> ") + RESET + cleanPrompt +
               " (" + std::to_string(min) + "-" + std::to_string(max) + "): " +
               BOLD + value + RESET;
    };

    if (!isatty(STDIN_FILENO)) {
        printOptionBox(title, rows);
        int val;
        while (true) {
            std::string cleanPrompt = ltrimPrompt(prompt);
            std::string plain = ">> " + cleanPrompt + " (" + std::to_string(min) + "-" + std::to_string(max) + "): ";
            std::cout << "\n" << indentFor(visLen(plain))
                      << B3 << ">> " << RESET << cleanPrompt << " (" << min << "-" << max << "): ";
            if (!(std::cin >> val)) {
                if (std::cin.eof()) return min;
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                printCentered(RED + std::string("Enter a number between ") +
                              std::to_string(min) + " and " + std::to_string(max) + "." + RESET);
                continue;
            }
            if (val >= min && val <= max && rowNumberExists(rows, val)) {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                return val;
            }
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            printCentered(RED + std::string("Enter a number between ") +
                          std::to_string(min) + " and " + std::to_string(max) + "." + RESET);
        }
    }

    int selectedIndex = optionIndexFor(rows, min);
    if (selectedIndex < 0) selectedIndex = 0;
    std::string typed;
    int drawnLines = 0;

    termios oldTerm {};
    tcgetattr(STDIN_FILENO, &oldTerm);
    termios raw = oldTerm;
    raw.c_lflag &= static_cast<unsigned long>(~(ICANON | ECHO));
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    auto restore = [&]() { tcsetattr(STDIN_FILENO, TCSANOW, &oldTerm); };
    auto selectedNumber = [&]() { return std::stoi(rows[static_cast<size_t>(selectedIndex)].first); };
    auto render = [&]() {
        if (drawnLines > 0) {
            std::cout << "\033[" << drawnLines << "A\033[J";
        }
        drawnLines = printOptionBox(title, rows, selectedIndex, choiceLine(selectedNumber(), typed));
        std::cout.flush();
    };

    render();
    while (true) {
        char ch = 0;
        if (read(STDIN_FILENO, &ch, 1) != 1) continue;
        if (ch == '\n' || ch == '\r') {
            if (!typed.empty()) {
                try {
                    int val = std::stoi(typed);
                    if (val >= min && val <= max && rowNumberExists(rows, val)) {
                        restore();
                        std::cout << "\n";
                        return val;
                    }
                } catch (...) {}
                typed.clear();
                render();
                continue;
            }
            restore();
            std::cout << "\n";
            return selectedNumber();
        }
        if (ch == 27) {
            char seq[2] = {0, 0};
            if (read(STDIN_FILENO, &seq[0], 1) == 1 &&
                read(STDIN_FILENO, &seq[1], 1) == 1 && seq[0] == '[') {
                typed.clear();
                if (seq[1] == 'A') selectedIndex = (selectedIndex <= 0) ? static_cast<int>(rows.size()) - 1 : selectedIndex - 1;
                if (seq[1] == 'B') selectedIndex = (selectedIndex + 1 >= static_cast<int>(rows.size())) ? 0 : selectedIndex + 1;
                render();
            }
        } else if (std::isdigit(static_cast<unsigned char>(ch))) {
            typed += ch;
            render();
        } else if ((ch == 127 || ch == 8) && !typed.empty()) {
            typed.pop_back();
            render();
        }
    }
}



// ── Double-Enter cancel helper ───────────────────────────────────────────────
// Returns "" if user cancelled (pressed Enter twice), otherwise returns value
static std::string getFieldOrCancel(const std::string& prompt, bool& cancelled) {
    if (cancelled) return "";
    printInputPrompt(prompt);
    std::string val;
    std::getline(std::cin, val);
    if (!val.empty()) return val;
    // First empty — warn
    printInputPrompt(std::string(YELLOW) + "(Empty! Press Enter again to cancel, or type a value): " + RESET);
    std::getline(std::cin, val);
    if (val.empty()) {
        cancelled = true;
        return "";
    }
    return val;
}

static bool idTaken(const std::vector<std::string>& ids, const std::string& id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

static std::string formatMovieId(int number) {
    std::ostringstream oss;
    oss << "MOV" << std::setw(4) << std::setfill('0') << number;
    return oss.str();
}

static int movieIdNumber(const std::string& id) {
    if (id.size() != 7 || id.rfind("MOV", 0) != 0) return 0;
    if (!std::all_of(id.begin() + 3, id.end(), [](unsigned char ch) { return std::isdigit(ch); })) return 0;
    return std::stoi(id.substr(3));
}

static std::string nextMovieIdFromTaken(const std::vector<std::string>& taken) {
    int maxNumber = 0;
    for (const auto& id : taken) {
        maxNumber = std::max(maxNumber, movieIdNumber(id));
    }
    return formatMovieId(maxNumber + 1);
}

static std::string nextMovieId(const std::vector<Movie>& movies) {
    std::vector<std::string> taken;
    for (const auto& movie : movies) {
        if (!movie.id.empty()) taken.push_back(movie.id);
    }
    return nextMovieIdFromTaken(taken);
}

static bool parseShowtime(const std::string& showtime, std::time_t& out) {
    if (showtime.size() < 16) return false;
    std::tm tm {};
    std::istringstream ss(showtime);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M");
    if (ss.fail()) return false;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    out = std::mktime(&tm);
    return out != static_cast<std::time_t>(-1);
}

static std::string currentDateTime() {
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return buf;
}

static std::string startsInLabel(std::time_t showtime) {
    std::time_t now = std::time(nullptr);
    long seconds = static_cast<long>(std::difftime(showtime, now));
    if (seconds <= 0) return "Now showing";
    int days = static_cast<int>(seconds / 86400);
    int hours = static_cast<int>((seconds % 86400) / 3600);
    int mins = static_cast<int>((seconds % 3600) / 60);
    std::ostringstream out;
    if (days > 0) out << days << "d " << hours << "h";
    else if (hours > 0) out << hours << "h " << mins << "m";
    else out << mins << "m";
    return out.str();
}

static bool repairDuplicateMovieIds(std::vector<Movie>& movies, std::vector<Ticket>& tickets) {
    std::vector<std::string> taken;
    for (const auto& movie : movies) {
        if (!movie.id.empty() && !idTaken(taken, movie.id)) taken.push_back(movie.id);
    }

    bool changed = false;
    std::vector<std::string> seen;
    for (auto& movie : movies) {
        bool duplicate = movie.id.empty() || idTaken(seen, movie.id);
        if (!duplicate) {
            seen.push_back(movie.id);
            continue;
        }

        std::string oldId = movie.id;
        std::string newId = nextMovieIdFromTaken(taken);
        movie.id = newId;
        taken.push_back(newId);
        seen.push_back(newId);
        changed = true;

        // When duplicated IDs exist, match tickets by both old ID and title where possible.
        for (auto& ticket : tickets) {
            if (ticket.movieId == oldId && ticket.movieTitle == movie.title) {
                ticket.movieId = newId;
            }
        }
    }
    return changed;
}

MenuManager::MenuManager(AuthManager& auth) : auth(auth) {
    movies  = DataManager::loadMovies();
    tickets = DataManager::loadTickets();
    halls   = DataManager::loadHalls();
    if (repairDuplicateMovieIds(movies, tickets)) {
        try {
            DataManager::saveMovies(movies);
            DataManager::saveTickets(tickets);
        } catch (...) {}
    }
    DataManager::loadRatings(movies);

    // ── Auto-add a "starting soon" demo movie based on current timestamp ──
    // This is the timestamp feature: showtime = now + 30 minutes
    {
        time_t nowT = time(0);
        time_t soonT = nowT + 30*60; // 30 minutes from now
        tm* st = localtime(&soonT);
        char showtimeBuf[20];
        strftime(showtimeBuf, sizeof(showtimeBuf), "%Y-%m-%d %H:%M", st);
        std::string generatedShowtime(showtimeBuf);

        // Check if we already have this auto-generated movie
        bool found = false;
        for (auto& m : movies)
            if (m.id == "MOV_DEMO") { 
                // Update its showtime to stay 30 min ahead
                m.showtime = generatedShowtime;
                found = true; break; 
            }
        if (!found && !movies.empty()) {
            // Pick a random movie title from a list
            std::vector<std::string> demoTitles = {
                "The Dark Knight","Interstellar","Inception",
                "The Matrix","Titanic","Jurassic Park",
                "Harry Potter","Fast & Furious","Black Panther"
            };
            std::string title = demoTitles[rand() % demoTitles.size()];
            std::string hallId = halls.empty() ? "HALL_A" : halls[rand()%halls.size()].id;
            Hall* h = findHall(hallId);
            int seats = h ? h->totalSeats() : 40;
            movies.push_back(Movie("MOV_DEMO", title, "Featured",
                "Auto-generated: starting soon based on current timestamp.",
                generatedShowtime, hallId, seats, 10.00));
            DataManager::saveMovies(movies);
        }
    }

    // Seed random with timestamp
    srand((unsigned int)time(0));

    // Seed default halls if none exist
    if (halls.empty()) {
        halls.push_back(Hall("HALL_A", "Hall A", 5, 8));  // 5 rows x 8 cols = 40 seats
        halls.push_back(Hall("HALL_B", "Hall B", 4, 6));  // 4 rows x 6 cols = 24 seats
        halls.push_back(Hall("HALL_C", "Hall C", 6,10));  // 6 rows x 10 cols = 60 seats
        DataManager::saveHalls(halls);
    }

    // Seed default movies if none exist
    if (movies.empty()) {
        movies.push_back(Movie("MOV0001","Avengers: Doomsday","Action",
            "Earth's mightiest heroes face their greatest threat yet.",
            "2025-06-01 14:00","HALL_A",halls[0].totalSeats(),12.50));
        movies.push_back(Movie("MOV0002","Inside Out 3","Animation",
            "Riley navigates new emotions in her college years.",
            "2025-06-01 17:00","HALL_B",halls[1].totalSeats(),9.00));
        movies.push_back(Movie("MOV0003","Mission Impossible 8","Thriller",
            "Ethan Hunt races against time to stop a rogue AI.",
            "2025-06-02 20:00","HALL_C",halls[2].totalSeats(),15.00));
        movies.push_back(Movie("MOV0004","Lilo & Stitch","Family",
            "A lonely Hawaiian girl befriends a mischievous alien.",
            "2025-06-03 11:00","HALL_A",halls[0].totalSeats(),8.50));
        DataManager::saveMovies(movies);
    }
}

// ════════════════════════════════════════════════════════
//  MAIN LOOP
// ════════════════════════════════════════════════════════
void MenuManager::run() {
    while (true) {
        clearScreen(); printBanner();
        int choice = chooseOptionBox("Portal Options", {
            {"1", "🔐 Login Account"},
            {"2", "📝 Register Account"},
            {"0", "🚪 Exit"}
        }, "Choice", 0, 2);
        if      (choice==0) {
            clearScreen();
            std::cout << "\n";
            for (const auto& artLine : goodbyeMarqueeLines("GOODBYE")) {
                printCentered(artLine);
            }
            printCentered(std::string(B4) + BOLD + "🎬 Homework later… movie first 😌" + RESET);
            printCentered(std::string(B5) + BOLD + "🍿 The movie may end, but the memories continue." + RESET);
            std::cout << "\n";
            break;
        }
        else if (choice==1) { if (doLogin()) runUserSession(); }
        else if (choice==2) doRegister();
    }
}

bool MenuManager::doLogin() {
    clearScreen(); printHeader("LOGIN");
    int roleChoice = chooseOptionBox("Login As", {
        {"1", "🛠️ Admin"},
        {"2", "🍿 Customer"},
        {"0", "⬅️ Back"}
    }, "Role", 0, 2);
    if (roleChoice==0) return false;
    Role expectedRole = (roleChoice==1)?Role::ADMIN:Role::CUSTOMER;
    for (int attempts=0; attempts<3; attempts++) {
        std::cout<<"\n";
        std::string username=getStringInput("  Username: ");
        std::string password=getStringInput("  Password: ");
        currentUser = auth.login(username, password);
        if (currentUser && currentUser->role==expectedRole) {
            const int boxW = 58;
            const std::string welcome = "Welcome, " + currentUser->displayName + " [ " + currentUser->roleToString() + " ]";
            const std::string I = indentFor(boxW + 2);
            std::cout << "\n" << MBLUE << I << "┌" << repeat("─", boxW) << "┐\n" << RESET;
            std::cout << MBLUE << I << "│" << RESET << PINK << BOLD << centerIn(welcome, boxW)
                      << RESET << MBLUE << "│\n" << RESET;
            std::cout << DPINK << I << "└" << repeat("─", boxW) << "┘\n" << RESET;
            std::this_thread::sleep_for(std::chrono::milliseconds(450));
            showInlineLoading("LOADING " + currentUser->roleToString() + " MODE",
                              "Ready! Opening " + currentUser->roleToString() + " dashboard...");
            return true;
        } else if (currentUser) {
            std::cout<<RED<<"  Wrong role for this account.\n"<<RESET; currentUser=nullptr;
        } else {
            std::cout<<RED<<"  Invalid credentials. ("<<(2-attempts)<<" left)\n"<<RESET;
        }
    }
    std::cout<<RED<<"  Too many failed attempts.\n"<<RESET;
    waitForEnter(); return false;
}

void MenuManager::doRegister() {
    clearScreen(); printHeader("REGISTER");
    int roleChoice = chooseOptionBox("Register As", {
        {"1", "🍿 Customer"},
        {"0", "⬅️ Back"}
    }, "Role", 0, 1);
    if (roleChoice==0) return;
    Role role = Role::CUSTOMER;
    std::cout<<"\n";
    std::string displayName=getStringInput("  Full Name: ");
    std::string username;
    while (true) {
        username=getStringInput("  Username: ");
        bool exists=false;
        for (auto* u:auth.getUsers()) if(u->username==username){exists=true;break;}
        if (exists) std::cout<<YELLOW<<"  Username taken, try another.\n"<<RESET;
        else break;
    }
    std::string password=getStringInput("  Password: ");
    std::string confirm=getStringInput("  Confirm Password: ");
    if (password!=confirm){std::cout<<RED<<"\n  Passwords do not match!\n"<<RESET;waitForEnter();return;}
    if (password.length()<4){std::cout<<RED<<"\n  Password too short (min 4).\n"<<RESET;waitForEnter();return;}
    auth.addUser(username,password,role,displayName);
    showLoading("CREATING CUSTOMER ACCOUNT", "Saving your profile and preparing Customer mode...");
    std::cout<<LGREEN<<"\n  Account created! You can now login.\n"<<RESET;
    waitForEnter();
}

void MenuManager::runUserSession() {
    while (true) {
        clearScreen(); printHeader("MOVIE TICKET MANAGEMENT SYSTEM");
        std::string roleArtColor =
            currentUser->role == Role::ADMIN ? std::string("\033[38;2;0;92;255m") :
            currentUser->role == Role::CUSTOMER ? std::string("\033[38;2;199;21;133m") :
            std::string(B3);
        printBlockText("WELCOME", roleArtColor);
        printBlockText(currentUser->roleToString(), roleArtColor);
        printCentered(std::string(MBLUE)+BOLD+"Hello, "+currentUser->displayName+RESET);
        printCentered(std::string(LPINK)+"Role: "+currentUser->roleToString()+RESET);
        printSeparator();
        // Show correct max based on role
        int maxChoice = (currentUser->role==Role::ADMIN) ? 6 :
                        (currentUser->role==Role::CUSTOMER) ? 11 : 5;
        std::vector<std::pair<std::string, std::string>> rows;
        std::string title;
        if (currentUser->role==Role::ADMIN) {
            title = "Admin Options";
            rows = {
                {"1", "🎬 Manage Movies"},
                {"2", "🏛️ Manage Halls"},
                {"3", "🎟️ View All Tickets"},
                {"4", "👤 Manage Users"},
                {"5", "📊 Reports"},
                {"6", "🔍 Search Movies"},
                {"0", "🚪 Logout"}
            };
        } else if (currentUser->role==Role::CUSTOMER) {
            title = "Customer Options";
            rows = {
                {"1", "🍿 Browse Movies"},
                {"2", "⭐ Movie Details & Reviews"},
                {"3", "🪑 View by Hall & Seat Map"},
                {"4", "🎟️ Book Ticket"},
                {"5", "🎫 My Tickets"},
                {"6", "❌ Cancel Ticket"},
                {"7", "💬 Rate a Movie"},
                {"8", "🕘 Booking History"},
                {"9", "🔍 Search Movies"},
                {"10", "⏰ Time Recommended Movies"},
                {"11", "🧾 Ticket Details / Receipt"},
                {"0", "🚪 Logout"}
            };
        } else {
            title = "Staff Options";
            rows = {
                {"1", "🎬 View Movies"},
                {"2", "➕ Add Movie"},
                {"3", "✏️ Edit Movie"},
                {"4", "🎟️ View All Tickets"},
                {"5", "🏛️ View Halls"},
                {"0", "🚪 Logout"}
            };
        }
        int choice=chooseOptionBox(title, rows, "Choice",0,maxChoice);
        if (choice==0){currentUser=nullptr;std::cout<<"\n"; printCentered(std::string(YELLOW)+"Logged out."+RESET);waitForEnter();return;}
        if      (currentUser->role==Role::ADMIN)    dispatchAdmin(choice);
        else                                         dispatchCustomer(choice);
    }
}

void MenuManager::dispatchAdmin(int c) {
    if (c==6) { searchMovies(); return; }  // quick search from main admin menu
    if (c==1) {
        clearScreen(); printHeader("MANAGE MOVIES");
        int x=chooseOptionBox("Movie Tools", {
            {"1", "📋 List Movies"},
            {"2", "➕ Add Movie"},
            {"3", "✏️ Edit Movie"},
            {"4", "🗑️ Delete Movie"},
            {"5", "🔍 Search Movies"},
            {"0", "⬅️ Back"}
        }, "Choice", 0, 5);
        if(x==1) listMovies(); else if(x==2) addMovie();
        else if(x==3) editMovie(); else if(x==4) deleteMovie(); else if(x==5) searchMovies();
    }
    else if (c==2) {
        clearScreen(); printHeader("MANAGE HALLS");
        int x=chooseOptionBox("Hall Tools", {
            {"1", "📋 List Halls"},
            {"2", "➕ Add Hall"},
            {"3", "✏️ Edit Hall"},
            {"4", "🗑️ Delete Hall"},
            {"5", "🪑 View Seat Map"},
            {"0", "⬅️ Back"}
        }, "Choice", 0, 5);
        if(x==1) listHalls();
        else if(x==2) addHall();
        else if(x==3) editHall();
        else if(x==4) deleteHall();
        else if(x==5) {
            clearScreen(); printHeader("VIEW HALL SEAT MAP");
            tabulate::Table ht;
            ht.addHeader({"Hall ID","Name","Total","Booked","Available"});
            for(auto& h:halls)
                ht.addRow({h.id, h.name,
                           std::to_string(h.totalSeats()),
                           std::to_string(h.bookedSeats()),
                           std::to_string(h.availableSeats())});
            std::cout<<"\n"; ht.print();
            std::string hid = getStringInput("\n  Enter Hall ID to view seat map (or Enter to cancel): ");
            if (!hid.empty()) {
                std::transform(hid.begin(),hid.end(),hid.begin(),::toupper);
                Hall* hl = findHall(hid);
                if (!hl) std::cout<<RED<<"  Hall not found!\n"<<RESET;
                else viewHallSeats(hid, "");
            }
            waitForEnter();
        }
    }
    else if (c==3) viewAllTickets();
    else if (c==4) manageUsers();
    else if (c==5) viewReports();
}

void MenuManager::dispatchStaff(int c) {
    if(c==1) listMovies(); else if(c==2) addMovie();
    else if(c==3) editMovie(); else if(c==4) viewAllTickets();
    else if(c==5) listHalls();
}

void MenuManager::dispatchCustomer(int c) {
    if(c==1) listMovies(false); else if(c==2) viewMovieDetail();
    else if(c==3) viewByHall();  else if(c==4) bookTicket();
    else if(c==5) viewMyTickets(); else if(c==6) cancelTicket();
    else if(c==7) rateMovie();   else if(c==8) viewHistory();
    else if(c==9) searchMovies();
    else if(c==10) showTimeRecommendations();
    else if(c==11) viewTicketDetails();
}

// ════════════════════════════════════════════════════════
//  HALL SEAT MAP — the key feature!
// ════════════════════════════════════════════════════════
void MenuManager::viewHallSeats(const std::string& hallId, const std::string& movieId) {
    Hall* h = findHall(hallId);
    if (!h) { printCentered(RED+std::string("Hall not found!")+RESET); return; }

    if (movieId.empty()) {
        clearScreen();
    }
    printHeader(h->name + " - Seat Map");

    constexpr int rowLabelW = 4;
    constexpr int seatW = 10;
    const int mapW = rowLabelW + 2 + h->cols * seatW;
    auto seatCell = [&](const std::string& text, const std::string& color) {
        return color + BOLD + centerIn(text, seatW) + RESET;
    };

    std::cout<<"\n";
    printCentered(std::string("Legend:  ") +
                  GREEN + BOLD + "[ FREE ]" + RESET + "  =  Available    " +
                  RED + BOLD + "[ X ]" + RESET + "  =  Taken");
    std::cout<<"\n";

    // Column header
    std::ostringstream header;
    header << std::string(rowLabelW + 2, ' ');
    for (int c=0; c<h->cols; c++)
        header << centerIn(std::to_string(c+1), seatW);
    std::cout << indentFor(mapW) << header.str() << "\n";

    std::cout << indentFor(mapW) << repeat("─", rowLabelW + 2 + h->cols * seatW) << "\n";

    // Each row
    for (int r=0; r<h->rows; r++) {
        char rowLetter = 'A' + r;
        std::ostringstream row;
        row << " " << rowLetter << "  | ";
        for (int c=0; c<h->cols; c++) {
            if (h->seats[r][c].booked)
                row << seatCell("[ X ]", RED);
            else
                row << seatCell("[FREE]", GREEN);
        }
        std::cout << indentFor(mapW) << row.str() << "\n";
    }

    std::cout<<"\n";
    std::ostringstream totals;
    totals << "Total: " << h->totalSeats()
           << "  |  Booked: " << h->bookedSeats()
           << "  |  Available: " << h->availableSeats();
    printCentered(DIM + totals.str() + RESET);
}

void MenuManager::listHalls() {
    clearScreen(); printHeader("CINEMA HALLS");

    tabulate::Table t;
    t.addHeader({"Hall ID","Name","Rows","Cols","Total Seats","Available"});
    for (auto& h : halls) {
        t.addRow({h.id, h.name,
                  std::to_string(h.rows), std::to_string(h.cols),
                  std::to_string(h.totalSeats()),
                  std::to_string(h.availableSeats())});
    }
    std::cout<<"\n"; t.print();
    waitForEnter();
}

void MenuManager::addHall() {
    clearScreen(); printHeader("ADD HALL");

    std::cout << DIM << "  Tip: Press Enter TWICE on any field to cancel\n" << RESET << "\n";

    bool cancelled = false;

    std::string id = getFieldOrCancel("  Hall ID (e.g. HALL_D): ", cancelled);
    if (cancelled) { std::cout<<YELLOW<<"\n  Cancelled. No hall added.\n"<<RESET; waitForEnter(); return; }
    std::transform(id.begin(),id.end(),id.begin(),::toupper);

    // Check duplicate
    if (findHall(id)) {
        std::cout<<RED<<"  Error: Hall '"<<id<<"' already exists!\n"<<RESET;
        waitForEnter(); return;
    }

    std::string name = getFieldOrCancel("  Hall Name (e.g. Hall D): ", cancelled);
    if (cancelled) { std::cout<<YELLOW<<"\n  Cancelled. No hall added.\n"<<RESET; waitForEnter(); return; }

    // Rows: double-enter cancels
    int rows = 0;
    while (!cancelled) {
        std::string rowStr = getFieldOrCancel("  Number of rows 1-26 (A,B,C...): ", cancelled);
        if (cancelled) break;
        try {
            rows = std::stoi(rowStr);
            if (rows>=1 && rows<=26) break;
            std::cout<<RED<<"  Error: Enter a number between 1 and 26\n"<<RESET;
        } catch (...) { std::cout<<RED<<"  Error: Numbers only please\n"<<RESET; }
    }
    if (cancelled) { std::cout<<YELLOW<<"\n  Cancelled. No hall added.\n"<<RESET; waitForEnter(); return; }

    // Cols: double-enter cancels
    int cols = 0;
    while (!cancelled) {
        std::string colStr = getFieldOrCancel("  Seats per row 1-30: ", cancelled);
        if (cancelled) break;
        try {
            cols = std::stoi(colStr);
            if (cols>=1 && cols<=30) break;
            std::cout<<RED<<"  Error: Enter a number between 1 and 30\n"<<RESET;
        } catch (...) { std::cout<<RED<<"  Error: Numbers only please\n"<<RESET; }
    }
    if (cancelled) { std::cout<<YELLOW<<"\n  Cancelled. No hall added.\n"<<RESET; waitForEnter(); return; }

    halls.push_back(Hall(id, name, rows, cols));
    DataManager::saveHalls(halls);
    std::cout<<GREEN<<"\n  Hall '"<<name<<"' added! ("<<rows*cols<<" total seats)\n"<<RESET;
    waitForEnter();
}

void MenuManager::editHall() {
    clearScreen(); printHeader("EDIT HALL");
    listHalls();
    std::string id = getStringInput("  Hall ID to edit: ");
    Hall* h = findHall(id);
    if (!h) { std::cout<<RED<<"  Hall not found!\n"<<RESET; waitForEnter(); return; }
    auto prompt=[](const std::string& label,const std::string& cur)->std::string{
        std::cout<<"  "<<label<<" ["<<cur<<"]: ";
        std::string in; std::getline(std::cin,in); return in.empty()?cur:in;
    };
    h->name = prompt("Name", h->name);
    std::string rowsStr = prompt("Rows", std::to_string(h->rows));
    std::string colsStr = prompt("Cols", std::to_string(h->cols));
    h->rows = std::stoi(rowsStr);
    h->cols = std::stoi(colsStr);
    h->initSeats();  // reset seats with new dimensions
    DataManager::saveHalls(halls);
    std::cout<<GREEN<<"\n  Hall updated!\n"<<RESET;
    waitForEnter();
}

void MenuManager::deleteHall() {
    clearScreen(); printHeader("DELETE HALL");
    listHalls();
    std::string id=getStringInput("  Hall ID to delete: ");
    auto it=std::find_if(halls.begin(),halls.end(),[&id](const Hall& h){return h.id==id;});
    if (it==halls.end()){std::cout<<RED<<"  Not found!\n"<<RESET;}
    else {
        std::cout<<RED<<"  Delete '"<<it->name<<"'? (y/n): "<<RESET;
        char c; std::cin>>c; std::cin.ignore();
        if (tolower(c)=='y'){halls.erase(it);DataManager::saveHalls(halls);std::cout<<GREEN<<"  Deleted.\n"<<RESET;}
        else std::cout<<YELLOW<<"  Cancelled.\n"<<RESET;
    }
    waitForEnter();
}

void MenuManager::viewByHall() {
    clearScreen(); printHeader("VIEW BY HALL");
    showStartingSoon();

    tabulate::Table t;
    t.addHeader({"Hall","Movie","Showtime","Available","Price"});
    for (auto& m : movies) {
        Hall* h = findHall(m.hallId);
        std::string hallName = h ? h->name : m.hallId;
        std::ostringstream price; price<<"$"<<std::fixed<<std::setprecision(2)<<m.price;
        std::string avail = m.isSoldOut() ?
            (RED+std::string("SOLD OUT")+RESET) :
            (GREEN+std::to_string(m.availableSeats())+RESET);
        t.addRow({hallName, m.title, m.showtime, avail, price.str()});
    }
    std::cout<<"\n"; t.print();

    // Let customer pick a hall to see seat map
    tabulate::Table hallPicker;
    hallPicker.addHeader({"Hall ID", "Name"});
    for (auto& h : halls) hallPicker.addRow({h.id, h.name});
    hallPicker.addRow({"0", "Back"});
    std::cout<<"\n";
    hallPicker.print();
    std::cout<<"\n";
    std::string hallChoice = getStringInput("  Hall ID (or 0 to back): ");
    if (hallChoice=="0") return;

    // Find a movie in that hall
    std::string movieId;
    for (auto& m : movies) if (m.hallId==hallChoice) { movieId=m.id; break; }

    viewHallSeats(hallChoice, movieId);
    waitForEnter();
}

// ════════════════════════════════════════════════════════
//  MOVIE FEATURES
// ════════════════════════════════════════════════════════
void MenuManager::listMovies(bool showAll) {
    clearScreen(); printHeader("MOVIE LISTINGS");
    if (movies.empty()){std::cout<<YELLOW<<"  No movies found.\n"<<RESET;waitForEnter();return;}
    // Show starting soon banner for customers
    if (currentUser && currentUser->role == Role::CUSTOMER) showStartingSoon();
    int page=0,pageSize=8,paginating=true;
    while (paginating) {
        int start=page*pageSize,end=std::min(start+pageSize,(int)movies.size());
        int totalPages=((int)movies.size()+pageSize-1)/pageSize;
        // Get today's date for timestamp comparison
        time_t nowt = time(0); tm* tmt = localtime(&nowt);
        char todayBuf[11]; strftime(todayBuf,sizeof(todayBuf),"%Y-%m-%d",tmt);
        std::string todayStr(todayBuf);

        tabulate::Table t;
        t.addHeader({"ID","Title","Genre","Hall","Showtime","Avail","Price","When"});
        for (int i=start;i<end;i++) {
            const Movie& m=movies[i];
            Hall* h=findHall(m.hallId);
            std::string hallName=h?h->name:m.hallId;
            std::ostringstream price; price<<"$"<<std::fixed<<std::setprecision(2)<<m.price;
            std::string avail=m.isSoldOut()?(RED+std::string("SOLD")+RESET):
                              (m.availableSeats()<=5?(YELLOW+std::to_string(m.availableSeats())+RESET):
                              (GREEN+std::to_string(m.availableSeats())+RESET));
            // Timestamp comparison
            std::string movieDate = m.showtime.size()>=10 ? m.showtime.substr(0,10) : "";
            std::string when;
            if      (movieDate == todayStr)   when = YELLOW+std::string("TODAY")+RESET;
            else if (movieDate >  todayStr)   when = CYAN+std::string("UPCOMING")+RESET;
            else                              when = DIM+std::string("EXPIRED")+RESET;
            t.addRow({m.id,m.title,m.genre,hallName,m.showtime,avail,price.str(),when});
        }
        std::cout<<"\n"; t.print();
        printCentered(std::string(DIM)+"Page "+std::to_string(page+1)+"/"+std::to_string(totalPages)+RESET);
        if (totalPages>1){
            std::cout<<"\n";
            printCentered(std::string(CYAN)+"[N]"+RESET+"ext page    "+
                          std::string(CYAN)+"[B]"+RESET+"ack page    "+
                          std::string(CYAN)+"[Q]"+RESET+"uit list");
            std::cout<<"\n  Choice: ";
            std::string nav; std::getline(std::cin,nav);
            if(!nav.empty()){char c=tolower(nav[0]);
                if(c=='n'&&page<totalPages-1)page++;
                else if(c=='b'&&page>0)page--;
                else paginating=false;
            } else paginating=false;
        } else paginating=false;
    }
    waitForEnter();
}

void MenuManager::viewMovieDetail() {
    clearScreen(); printHeader("MOVIE DETAILS");
    listMovies(false);
    std::string id=getStringInput("  Enter Movie ID: ");
    Movie* m=findMovie(id);
    if (!m){std::cout<<RED<<"  Not found!\n"<<RESET;waitForEnter();return;}
    clearScreen(); printHeader(m->title);
    Hall* h=findHall(m->hallId);
    std::ostringstream price;
    price << "$" << std::fixed << std::setprecision(2) << m->price;

    tabulate::Table details;
    details.addHeader({"Metric", "Value"});
    details.addRow({"Movie ID", m->id});
    details.addRow({"Title", m->title});
    details.addRow({"Hall", h?h->name:m->hallId});
    details.addRow({"Genre", m->genre});
    details.addRow({"Showtime", m->showtime});
    details.addRow({"Price", price.str()});
    details.addRow({"Seats Left", GREEN + std::to_string(m->availableSeats()) + RESET + "/" + std::to_string(m->totalSeats)});
    details.addRow({"Rating", m->starsStr()});
    details.addRow({"Description", m->description});
    std::cout << "\n";
    details.print();

    // Show seat map
    if (h) { std::cout<<"\n"; viewHallSeats(h->id, m->id); }

    if (!m->ratings.empty()){
        tabulate::Table t; t.addHeader({"User","Stars","Comment","Date"});
        for (auto& r:m->ratings){
            t.addRow({r.username,Movie::ratingIcons(r.stars),r.comment,r.date});
        }
        std::cout<<"\n"; t.print();
    } else std::cout<<YELLOW<<"\n  No reviews yet.\n"<<RESET;
    waitForEnter();
}

void MenuManager::addMovie() {
    clearScreen(); printHeader("ADD MOVIE");
    listHalls();

    std::cout << DIM << "  Tip: Press Enter TWICE on any field to cancel\n" << RESET << "\n";

    bool cancelled = false;
    Movie m;
    m.id = nextMovieId(movies);

    // ── Title ────────────────────────────────────────────
    m.title = getFieldOrCancel("  Title: ", cancelled);
    if (cancelled) { std::cout<<YELLOW<<"\n  Cancelled. No movie added.\n"<<RESET; waitForEnter(); return; }

    // ── Genre: letters only ──────────────────────────────
    while (!cancelled) {
        m.genre = getFieldOrCancel("  Genre (e.g. Action): ", cancelled);
        if (cancelled) break;
        bool ok = true;
        for (char c : m.genre) if (!isalpha(c) && c!=' ' && c!='&' && c!='-') { ok=false; break; }
        if (!ok) std::cout<<RED<<"  Error: Letters only! (e.g. Action, Sci-Fi)\n"<<RESET;
        else break;
    }
    if (cancelled) { std::cout<<YELLOW<<"\n  Cancelled. No movie added.\n"<<RESET; waitForEnter(); return; }

    // ── Description ──────────────────────────────────────
    m.description = getFieldOrCancel("  Description: ", cancelled);
    if (cancelled) { std::cout<<YELLOW<<"\n  Cancelled. No movie added.\n"<<RESET; waitForEnter(); return; }

    // ── Showtime ─────────────────────────────────────────
    while (!cancelled) {
        m.showtime = getFieldOrCancel("  Showtime (YYYY-MM-DD HH:MM): ", cancelled);
        if (cancelled) break;
        bool valid = m.showtime.size()==16 &&
                     isdigit(m.showtime[0]) && isdigit(m.showtime[1]) &&
                     isdigit(m.showtime[2]) && isdigit(m.showtime[3]) &&
                     m.showtime[4]=='-' &&
                     isdigit(m.showtime[5]) && isdigit(m.showtime[6]) &&
                     m.showtime[7]=='-' &&
                     isdigit(m.showtime[8]) && isdigit(m.showtime[9]) &&
                     m.showtime[10]==' ' &&
                     isdigit(m.showtime[11]) && isdigit(m.showtime[12]) &&
                     m.showtime[13]==':' &&
                     isdigit(m.showtime[14]) && isdigit(m.showtime[15]);
        if (valid) {
            int mon=std::stoi(m.showtime.substr(5,2)), day=std::stoi(m.showtime.substr(8,2));
            int hour=std::stoi(m.showtime.substr(11,2)), min=std::stoi(m.showtime.substr(14,2));
            valid=(mon>=1&&mon<=12)&&(day>=1&&day<=31)&&(hour>=0&&hour<=23)&&(min>=0&&min<=59);
        }
        if (valid) break;
        std::cout<<RED<<"  Error: Use exactly: YYYY-MM-DD HH:MM  e.g. 2026-06-15 14:30\n"<<RESET;
    }
    if (cancelled) { std::cout<<YELLOW<<"\n  Cancelled. No movie added.\n"<<RESET; waitForEnter(); return; }

    // ── Hall ID ───────────────────────────────────────────
    while (!cancelled) {
        m.hallId = getFieldOrCancel("  Hall ID: ", cancelled);
        if (cancelled) break;
        std::transform(m.hallId.begin(),m.hallId.end(),m.hallId.begin(),::toupper);
        if (findHall(m.hallId)) break;
        std::cout<<RED<<"  Error: Hall '"<<m.hallId<<"' not found!\n"<<RESET;
    }
    if (cancelled) { std::cout<<YELLOW<<"\n  Cancelled. No movie added.\n"<<RESET; waitForEnter(); return; }

    // ── Price ─────────────────────────────────────────────
    while (!cancelled) {
        std::string priceStr = getFieldOrCancel("  Ticket Price ($): ", cancelled);
        if (cancelled) break;
        try { m.price = std::stod(priceStr); if (m.price>=0) break; }
        catch (...) {}
        std::cout<<RED<<"  Error: Enter a valid price like 12.50\n"<<RESET;
    }
    if (cancelled) { std::cout<<YELLOW<<"\n  Cancelled. No movie added.\n"<<RESET; waitForEnter(); return; }

    Hall* h = findHall(m.hallId);
    m.totalSeats  = h ? h->totalSeats() : 50;
    m.bookedSeats = 0;
    movies.push_back(m); saveAll();
    std::cout<<GREEN<<"\n  " << PINK << "Movie '"<<m.title<<"' added! ID: "<<m.id<<"\n"<<RESET;
    waitForEnter();
}

void MenuManager::editMovie() {
    clearScreen(); printHeader("EDIT MOVIE"); listMovies();
    std::string id=getStringInput("  Movie ID to edit: ");
    Movie* m=findMovie(id);
    if (!m){std::cout<<RED<<"  Not found!\n"<<RESET;waitForEnter();return;}
    auto prompt=[](const std::string& label,const std::string& cur)->std::string{
        std::cout<<"  "<<label<<" ["<<cur<<"]: ";
        std::string in; std::getline(std::cin,in); return in.empty()?cur:in;
    };
    m->title       = prompt("Title",m->title);
    m->genre       = prompt("Genre",m->genre);
    m->description = prompt("Description",m->description);
    m->showtime    = prompt("Showtime",m->showtime);
    m->hallId      = prompt("Hall ID",m->hallId);
    m->price       = std::stod(prompt("Price",std::to_string(m->price)));
    saveAll();
    std::cout<<GREEN<<"\n  Movie updated!\n"<<RESET; waitForEnter();
}

void MenuManager::deleteMovie() {
    clearScreen(); printHeader("DELETE MOVIE"); listMovies();
    std::string id=getStringInput("  Movie ID to delete: ");
    auto it=std::find_if(movies.begin(),movies.end(),[&id](const Movie& m){return m.id==id;});
    if(it==movies.end()){std::cout<<RED<<"  Not found!\n"<<RESET;}
    else{
        std::cout<<RED<<"  Delete '"<<it->title<<"'? (y/n): "<<RESET;
        char c; std::cin>>c; std::cin.ignore();
        if(tolower(c)=='y'){movies.erase(it);saveAll();std::cout<<GREEN<<"  Deleted.\n"<<RESET;}
        else std::cout<<YELLOW<<"  Cancelled.\n"<<RESET;
    }
    waitForEnter();
}

void MenuManager::searchMovies() {
    while (true) {
        clearScreen(); printHeader("SEARCH & FILTER MOVIES");

        int filter = chooseOptionBox("Search Filters", {
            {"1", "🔎 Name / Title"},
            {"2", "🆔 Movie ID"},
            {"3", "🎭 Genre"},
            {"4", "📅 Date  (YYYY-MM-DD)"},
            {"5", "🏛️ Hall"},
            {"6", "💵 Max Price  ($)"},
            {"7", "✅ Show Only Available"},
            {"0", "⬅️ Back"}
        }, "Filter choice", 0, 7);
        if (filter == 0) return;

        tabulate::Table t;
        t.addHeader({"ID","Title","Genre","Hall","Showtime","Avail","Price","When"});
        int found = 0;

        // Get today for When column
        time_t nowt2 = time(0); tm* tmt2 = localtime(&nowt2);
        char todayBuf2[11]; strftime(todayBuf2,sizeof(todayBuf2),"%Y-%m-%d",tmt2);
        std::string todayStr2(todayBuf2);

        auto addMovieRow = [&](const Movie& m) {
            Hall* h = findHall(m.hallId);
            std::ostringstream price; price<<"$"<<std::fixed<<std::setprecision(2)<<m.price;
            std::string avail = m.isSoldOut()?(RED+std::string("SOLD")+RESET):
                                (GREEN+std::to_string(m.availableSeats())+RESET);
            std::string movieDate2 = m.showtime.size()>=10?m.showtime.substr(0,10):"";
            std::string when2 = movieDate2==todayStr2 ? (YELLOW+std::string("TODAY")+RESET) :
                                movieDate2>todayStr2  ? (CYAN+std::string("UPCOMING")+RESET) :
                                                        (DIM+std::string("EXPIRED")+RESET);
            t.addRow({m.id,m.title,m.genre,h?h->name:m.hallId,m.showtime,avail,price.str(),when2});
            found++;
        };

        if (filter == 1) {
            std::string q = getStringInput("  Enter title (or Enter to cancel): ");
            if (q.empty()) continue;
            std::transform(q.begin(),q.end(),q.begin(),::tolower);
            for (auto& m:movies) {
                std::string t2=m.title; std::transform(t2.begin(),t2.end(),t2.begin(),::tolower);
                if (t2.find(q)!=std::string::npos) addMovieRow(m);
            }
            if (found==0) printCentered(RED+std::string("No movies with title containing '")+q+"'"+RESET);

        } else if (filter == 2) {
            std::string q = getStringInput("  Enter Movie ID (or Enter to cancel): ");
            if (q.empty()) continue;
            std::transform(q.begin(),q.end(),q.begin(),::toupper);
            for (auto& m:movies) if (m.id==q) addMovieRow(m);
            if (found==0) printCentered(RED+std::string("No movie found with ID '")+q+"'"+RESET);

        } else if (filter == 3) {
            std::string q = getStringInput("  Enter genre (or Enter to cancel): ");
            if (q.empty()) continue;
            std::transform(q.begin(),q.end(),q.begin(),::tolower);
            for (auto& m:movies) {
                std::string g=m.genre; std::transform(g.begin(),g.end(),g.begin(),::tolower);
                if (g.find(q)!=std::string::npos) addMovieRow(m);
            }
            if (found==0) printCentered(RED+std::string("No movies with genre '")+q+"'"+RESET);

        } else if (filter == 4) {
            std::string q = getStringInput("  Enter date YYYY-MM-DD (or Enter to cancel): ");
            if (q.empty()) continue;
            // Validate date format
            bool validDate = q.size()==10 && isdigit(q[0])&&isdigit(q[1])&&isdigit(q[2])&&isdigit(q[3])
                             && q[4]=='-' && isdigit(q[5])&&isdigit(q[6])
                             && q[7]=='-' && isdigit(q[8])&&isdigit(q[9]);
            if (!validDate) { printCentered(RED+std::string("Invalid date format! Use YYYY-MM-DD  e.g. 2026-06-15")+RESET); waitForEnter(); continue; }
            for (auto& m:movies) if (m.showtime.size()>=10 && m.showtime.substr(0,10)==q) addMovieRow(m);
            if (found==0) printCentered(RED+std::string("No movies on date '")+q+"'"+RESET);

        } else if (filter == 5) {
            listHalls();
            std::string q = getStringInput("  Enter Hall ID (or Enter to cancel): ");
            if (q.empty()) continue;
            std::transform(q.begin(),q.end(),q.begin(),::toupper);
            bool hallExists = findHall(q) != nullptr;
            if (!hallExists) { printCentered(RED+std::string("Hall '")+q+"' not found!"+RESET); waitForEnter(); continue; }
            for (auto& m:movies) if (m.hallId==q) addMovieRow(m);
            if (found==0) printCentered(RED+std::string("No movies in hall '")+q+"'"+RESET);

        } else if (filter == 6) {
            std::string priceInput = getStringInput("  Max price e.g. 15.00 (or Enter to cancel): ");
            if (priceInput.empty()) continue;
            double maxPrice = 0;
            try { maxPrice = std::stod(priceInput); }
            catch (...) { printCentered(RED+std::string("Invalid price! Enter a number like 15.00")+RESET); waitForEnter(); continue; }
            for (auto& m:movies) if (m.price<=maxPrice) addMovieRow(m);
            if (found==0) printCentered(RED+std::string("No movies under $")+priceInput+RESET);

        } else if (filter == 7) {
            for (auto& m:movies) if (!m.isSoldOut() && m.availableSeats()>0) addMovieRow(m);
            if (found==0) printCentered(RED+std::string("All movies are sold out!")+RESET);
        }

        std::cout << "\n";
        if (found == 0)
            printCentered(std::string(YELLOW)+"No movies found for that filter."+RESET);
        else {
            t.print();
            std::cout << "\n";
            printCentered(std::string(GREEN)+"Found "+std::to_string(found)+" movie(s)."+RESET);
        }
        waitForEnter();
    }
}

void MenuManager::showTimeRecommendations() {
    clearScreen();
    printHeader("TIME RECOMMENDED MOVIES");

    struct RecommendedMovie {
        Movie* movie;
        std::time_t showtime;
        double secondsAway;
        std::string reason;
    };

    std::vector<RecommendedMovie> picks;
    const std::time_t now = std::time(nullptr);
    for (auto& movie : movies) {
        if (movie.isSoldOut()) continue;
        std::time_t movieTime = 0;
        if (!parseShowtime(movie.showtime, movieTime)) continue;

        double diff = std::difftime(movieTime, now);
        if (diff < -3 * 60 * 60) continue;

        std::string reason;
        if (diff <= 0) reason = "Now showing";
        else if (diff <= 60 * 60) reason = "Starting very soon";
        else if (diff <= 3 * 60 * 60) reason = "Perfect next show";
        else if (diff <= 24 * 60 * 60) reason = "Later today";
        else reason = "Upcoming pick";

        picks.push_back({&movie, movieTime, diff, reason});
    }

    std::sort(picks.begin(), picks.end(), [](const RecommendedMovie& a, const RecommendedMovie& b) {
        return a.showtime < b.showtime;
    });

    if (picks.empty()) {
        printCentered(std::string(YELLOW) + "No upcoming available movies to recommend right now." + RESET);
        waitForEnter();
        return;
    }

    tabulate::Table table;
    table.addHeader({"Rank", "ID", "Movie", "Genre", "Hall", "Showtime", "Starts In", "Why"});
    const int limit = std::min(8, static_cast<int>(picks.size()));
    for (int i = 0; i < limit; ++i) {
        Movie* movie = picks[i].movie;
        Hall* hall = findHall(movie->hallId);
        table.addRow({
            std::to_string(i + 1),
            movie->id,
            movie->title,
            movie->genre,
            hall ? hall->name : movie->hallId,
            movie->showtime,
            startsInLabel(picks[i].showtime),
            picks[i].reason
        });
    }

    std::cout << "\n";
    table.print();
    std::cout << "\n";
    printCentered(std::string(B4) + "Tip: choose a movie that starts soon and still has seats available." + RESET);
    std::string id = getStringInput("\n  Movie ID to view details (or Enter to go back): ");
    if (id.empty()) return;
    std::transform(id.begin(), id.end(), id.begin(), ::toupper);
    Movie* movie = findMovie(id);
    if (!movie) {
        std::cout << RED << "  Movie not found.\n" << RESET;
        waitForEnter();
        return;
    }

    clearScreen();
    printHeader("RECOMMENDED MOVIE DETAILS");
    tabulate::Table details;
    details.addHeader({"Detail", "Value"});
    Hall* hall = findHall(movie->hallId);
    std::ostringstream price;
    price << "$" << std::fixed << std::setprecision(2) << movie->price;
    details.addRow({"Movie ID", movie->id});
    details.addRow({"Title", movie->title});
    details.addRow({"Genre", movie->genre});
    details.addRow({"Hall", hall ? hall->name : movie->hallId});
    details.addRow({"Showtime", movie->showtime});
    details.addRow({"Seats Left", GREEN + std::to_string(movie->availableSeats()) + RESET});
    details.addRow({"Price", price.str()});
    details.addRow({"Rating", movie->starsStr()});
    details.addRow({"Why", "Recommended by showtime"});
    std::cout << "\n";
    details.print();
    waitForEnter();
}

// ════════════════════════════════════════════════════════
//  BOOK TICKET — pick specific seats
// ════════════════════════════════════════════════════════
void MenuManager::bookTicket() {
    clearScreen(); printHeader("BOOK TICKET");
    listMovies(false);
    std::string movieId=getStringInput("  Movie ID: ");
    Movie* m=findMovie(movieId);
    if(!m){std::cout<<RED<<"  Not found!\n"<<RESET;waitForEnter();return;}
    if(m->isSoldOut()){std::cout<<RED<<"  Sold out!\n"<<RESET;waitForEnter();return;}

    Hall* h=findHall(m->hallId);
    if(!h){std::cout<<RED<<"  Hall not found!\n"<<RESET;waitForEnter();return;}

    // Show seat map
    viewHallSeats(h->id, m->id);
    std::cout<<"\n  Price: $"<<std::fixed<<std::setprecision(2)<<m->price<<" per seat\n";

    // Pick seats
    std::vector<std::string> pickedSeats;
    std::cout<<"\n  Enter seat IDs one by one (e.g. A1, B3). Type 'done' when finished.\n";
    while (true) {
        std::string seatId=getStringInput("  Seat ID (or 'done'): ");
        if (seatId=="done"||seatId=="DONE") break;
        // Convert to uppercase
        std::transform(seatId.begin(),seatId.end(),seatId.begin(),::toupper);
        Seat* s=h->findSeat(seatId);
        if (!s){std::cout<<RED<<"  Seat "<<seatId<<" does not exist!\n"<<RESET; continue;}
        if (s->booked){std::cout<<RED<<"  Seat "<<seatId<<" is already taken! Choose another.\n"<<RESET; continue;}
        // Check not already picked in this session
        if(std::find(pickedSeats.begin(),pickedSeats.end(),seatId)!=pickedSeats.end()){
            std::cout<<YELLOW<<"  Already selected.\n"<<RESET; continue;
        }
        pickedSeats.push_back(seatId);
        std::cout<<GREEN<<"  Seat "<<seatId<<" selected! ("<<pickedSeats.size()<<" total)\n"<<RESET;
    }

    if (pickedSeats.empty()){std::cout<<YELLOW<<"  No seats selected.\n"<<RESET;waitForEnter();return;}

    double total=pickedSeats.size()*m->price;
    std::cout<<"\n  "<<YELLOW<<"Booking seats: ";
    for(auto& s:pickedSeats) std::cout<<s<<" ";
    std::cout<<"\n  Total: $"<<std::fixed<<std::setprecision(2)<<total<<RESET;
    std::cout<<"\n  Confirm? (y/n): ";
    char c; std::cin>>c; std::cin.ignore();
    if(tolower(c)!='y'){std::cout<<YELLOW<<"  Cancelled.\n"<<RESET;waitForEnter();return;}

    // Book seats in hall
    for(auto& seatId:pickedSeats) h->bookSeat(seatId,currentUser->username);
    m->bookedSeats+=(int)pickedSeats.size();

    // ── ONE TICKET PER SEAT ──────────────────────────────
    std::cout<<GREEN<<"\n  Booking confirmed! "<<pickedSeats.size()<<" ticket(s) issued:\n\n"<<RESET;
    std::vector<Ticket> issuedTickets;
    for(auto& seatId:pickedSeats) {
        std::vector<std::string> singleSeat = {seatId};
        Ticket tk(DataManager::generateId("TKT",tickets.size()),
                  m->id, m->title, currentUser->username,
                  m->showtime, h->id, singleSeat,
                  m->price, DataManager::todayDate());
        tickets.push_back(tk);
        issuedTickets.push_back(tk);
        std::cout<<GREEN<<"  Ticket ID: "<<tk.ticketId<<"  |  Seat: "<<seatId
                 <<"  |  $"<<std::fixed<<std::setprecision(2)<<m->price<<"\n"<<RESET;
    }
    saveAll();
    showInlineLoading("GENERATING TICKET RECEIPT",
                      "Digital movie pass ready. Printing your receipt...");
    for (size_t i = 0; i < issuedTickets.size(); ++i) {
        std::string receiptPath = saveTicketReceipt(issuedTickets[i]);
        printTicketReceipt(issuedTickets[i], true);
        printCentered(std::string(GREEN) + "Receipt saved automatically: " + receiptPath + RESET);
    }
    waitForEnter();
}

void MenuManager::cancelTicket() {
    clearScreen(); printHeader("CANCEL TICKET"); viewMyTickets();
    std::string tid=getStringInput("  Ticket ID to cancel: ");
    Ticket* t=findTicket(tid);
    if(!t||t->customerUsername!=currentUser->username){std::cout<<RED<<"  Not found.\n"<<RESET;waitForEnter();return;}
    if(t->status=="Cancelled"){std::cout<<YELLOW<<"  Already cancelled.\n"<<RESET;waitForEnter();return;}
    std::cout<<RED<<"  Cancel '"<<t->movieTitle<<"' seats ["<<t->seatsStr()<<"]? (y/n): "<<RESET;
    char c; std::cin>>c; std::cin.ignore();
    if(tolower(c)=='y'){
        t->status="Cancelled";
        // Free up seats in hall
        Hall* h=findHall(t->hallId);
        if(h) for(auto& s:t->seats) h->cancelSeat(s);
        Movie* m=findMovie(t->movieId);
        if(m) m->bookedSeats=std::max(0,m->bookedSeats-t->seatCount);
        saveAll(); std::cout<<GREEN<<"  Cancelled. Seats freed.\n"<<RESET;
    } else std::cout<<YELLOW<<"  Not cancelled.\n"<<RESET;
    waitForEnter();
}

void MenuManager::viewMyTickets() {
    clearScreen(); printHeader("MY TICKETS");
    tabulate::Table t; t.addHeader({"ID","Movie","Hall","Seats","Total","Date","Status"});
    int count=0;
    for(auto& tk:tickets){
        if(tk.customerUsername!=currentUser->username) continue;
        std::ostringstream price; price<<"$"<<std::fixed<<std::setprecision(2)<<tk.totalPrice;
        std::string status=(tk.status=="Cancelled")?(RED+tk.status+RESET):(GREEN+tk.status+RESET);
        t.addRow({tk.ticketId,tk.movieTitle,tk.hallId,tk.seatsStr(),price.str(),tk.bookingDate,status});
        count++;
    }
    std::cout<<"\n";
    if(count==0) std::cout<<YELLOW<<"  No tickets yet.\n"<<RESET;
    else t.print();
    waitForEnter();
}

void MenuManager::viewHistory() {
    clearScreen(); printHeader("BOOKING HISTORY");
    tabulate::Table t; t.addHeader({"ID","Movie","Hall","Seats","Total","Date","Status"});
    int count=0;
    for(auto& tk:tickets){
        if(tk.customerUsername!=currentUser->username) continue;
        std::ostringstream price; price<<"$"<<std::fixed<<std::setprecision(2)<<tk.totalPrice;
        std::string status=(tk.status=="Cancelled")?(RED+tk.status+RESET):(GREEN+tk.status+RESET);
        t.addRow({tk.ticketId,tk.movieTitle,tk.hallId,tk.seatsStr(),price.str(),tk.bookingDate,status});
        count++;
    }
    std::cout<<"\n";
    if(count==0) std::cout<<YELLOW<<"  No booking history.\n"<<RESET;
    else{t.print(); std::cout<<"\n  Total bookings: "<<count<<"\n";}
    waitForEnter();
}

void MenuManager::printTicketReceipt(const Ticket& tk, bool showTitle) {
    if (showTitle) printHeader("TICKET RECEIPT");

    Movie* movie = findMovie(tk.movieId);
    Hall* hall = findHall(tk.hallId);
    std::ostringstream total;
    total << "$" << std::fixed << std::setprecision(2) << tk.totalPrice;
    std::ostringstream seatPrice;
    double each = tk.seatCount > 0 ? tk.totalPrice / tk.seatCount : tk.totalPrice;
    seatPrice << "$" << std::fixed << std::setprecision(2) << each;
    std::string customerName = currentUser && currentUser->username == tk.customerUsername
        ? currentUser->displayName
        : tk.customerUsername;

    std::cout << "\n";
    const int boxW = 56;
    const std::string I = indentFor(boxW + 2);
    auto border = [&](const std::string& left, const std::string& fill, const std::string& right) {
        std::cout << B2 << BOLD << I << left << repeat(fill, boxW) << right << RESET << "\n";
    };
    auto row = [&](const std::string& text, const std::string& color = WHITE, bool bold = false) {
        std::cout << B2 << BOLD << I << "║" << RESET
                  << color << (bold ? BOLD : "") << centerIn(text, boxW) << RESET
                  << B2 << BOLD << "║" << RESET << "\n";
    };
    auto field = [&](const std::string& icon, const std::string& label,
                     const std::string& value, const std::string& color = WHITE) {
        std::string iconCell = padRightVisible(" " + icon, 4);
        std::string labelCell = padRightVisible(label, 10);
        std::cout << B2 << BOLD << I << "║" << RESET
                  << padRightVisible(B5 + iconCell + labelCell + " : " + RESET
                                      + color + BOLD + value + RESET,
                                      boxW)
                  << B2 << BOLD << "║" << RESET << "\n";
    };

    std::string show = movie ? movie->showtime : tk.showtime;
    std::string date = show.size() >= 10 ? show.substr(0, 10) : tk.bookingDate;
    std::string time = show.size() >= 16 ? show.substr(11, 5) : show;
    std::string status = tk.status == "Cancelled" ? "CANCELLED ❌" : "CONFIRMED ✅";
    std::string statusColor = tk.status == "Cancelled" ? std::string(RED) : std::string(GREEN);

    border("╔", "═", "╗");
    row("███ DIGITAL MOVIE PASS ███", B3, true);
    row("🎬 MOVIE4Q4 🎬", B4, true);
    row("DIGITAL TICKET", B5, true);
    border("╠", "═", "╣");
    field("🧾", "Ticket ID", tk.ticketId, B3);
    field("👤", "User", customerName, B4);
    field("🎬", "Movie", tk.movieTitle, "\033[38;2;199;21;133m");
    field("🏢", "Hall", hall ? hall->name : tk.hallId, B4);
    field("💺", "Seat", tk.seatsStr(), GREEN);
    field("📅", "Date", date, WHITE);
    field("🕒", "Time", time, WHITE);
    field("💵", "Price", total.str(), GREEN);
    border("╠", "═", "╣");
    field("✅", "Status", status, statusColor);
    field("🕘", "Generated", currentDateTime(), DIM);
    border("╠", "═", "╣");
    row("🍿 Enjoy Your Movie Time 🍿", "\033[38;2;148;0;211m", true);
    border("╚", "═", "╝");
}

std::string MenuManager::saveTicketReceipt(const Ticket& tk) {
    std::filesystem::path dir = std::filesystem::path(DataManager::TICKETS_FILE).parent_path() / "receipts";
    std::filesystem::create_directories(dir);
    std::filesystem::path path = dir / (tk.ticketId + ".txt");

    Movie* movie = findMovie(tk.movieId);
    Hall* hall = findHall(tk.hallId);
    std::ostringstream total;
    total << "$" << std::fixed << std::setprecision(2) << tk.totalPrice;
    double each = tk.seatCount > 0 ? tk.totalPrice / tk.seatCount : tk.totalPrice;
    std::ostringstream seatPrice;
    seatPrice << "$" << std::fixed << std::setprecision(2) << each;

    std::ofstream out(path);
    std::string show = movie ? movie->showtime : tk.showtime;
    std::string date = show.size() >= 10 ? show.substr(0, 10) : tk.bookingDate;
    std::string time = show.size() >= 16 ? show.substr(11, 5) : show;

    const int savedW = 42;
    auto fileBorder = [&](const std::string& left, const std::string& right) {
        out << left << repeat("═", savedW) << right << "\n";
    };
    auto fileRow = [&](const std::string& text) {
        out << "║" << padRightVisible(text, savedW) << "║\n";
    };

    fileBorder("╔", "╗");
    fileRow(centerIn("MOVIE4Q4 DIGITAL TICKET", savedW));
    fileBorder("╠", "╣");
    fileRow(" Ticket ID : " + tk.ticketId);
    fileRow(" User      : " + tk.customerUsername);
    fileRow(" Movie     : " + tk.movieTitle);
    fileRow(" Hall      : " + (hall ? hall->name : tk.hallId));
    fileRow(" Seat      : " + tk.seatsStr());
    fileRow(" Date      : " + date);
    fileRow(" Time      : " + time);
    fileRow(" Price     : " + total.str());
    fileRow(" Status    : " + tk.status);
    fileRow(" Generated : " + currentDateTime());
    fileBorder("╠", "╣");
    fileRow(centerIn("Enjoy Your Movie Time", savedW));
    fileBorder("╚", "╝");
    return path.string();
}

void MenuManager::viewTicketDetails() {
    clearScreen();
    printHeader("TICKET DETAILS & RECEIPT");

    tabulate::Table table;
    table.addHeader({"ID","Movie","Hall","Seats","Total","Date","Status"});
    int count = 0;
    for (auto& tk : tickets) {
        if (tk.customerUsername != currentUser->username) continue;
        std::ostringstream price;
        price << "$" << std::fixed << std::setprecision(2) << tk.totalPrice;
        std::string status = tk.status == "Cancelled" ? RED + tk.status + RESET : GREEN + tk.status + RESET;
        table.addRow({tk.ticketId, tk.movieTitle, tk.hallId, tk.seatsStr(), price.str(), tk.bookingDate, status});
        count++;
    }

    std::cout << "\n";
    if (count == 0) {
        std::cout << YELLOW << "  No tickets yet.\n" << RESET;
        waitForEnter();
        return;
    }

    table.print();
    std::string ticketId = getStringInput("\n  Ticket ID for receipt (or Enter to go back): ");
    if (ticketId.empty()) return;
    std::transform(ticketId.begin(), ticketId.end(), ticketId.begin(), ::toupper);

    Ticket* ticket = findTicket(ticketId);
    if (!ticket || ticket->customerUsername != currentUser->username) {
        std::cout << RED << "  Ticket not found.\n" << RESET;
        waitForEnter();
        return;
    }

    clearScreen();
    printTicketReceipt(*ticket, true);
    std::string path = saveTicketReceipt(*ticket);
    std::cout << "\n";
    printCentered(std::string(GREEN) + "Receipt saved automatically: " + path + RESET);
    waitForEnter();
}

void MenuManager::viewAllTickets() {
    clearScreen(); printHeader("ALL TICKETS");
    tabulate::Table t; t.addHeader({"ID","Movie","Customer","Hall","Seats","Total","Status"});
    for(auto& tk:tickets){
        std::ostringstream price; price<<"$"<<std::fixed<<std::setprecision(2)<<tk.totalPrice;
        std::string status=(tk.status=="Cancelled")?(RED+tk.status+RESET):(GREEN+tk.status+RESET);
        t.addRow({tk.ticketId,tk.movieTitle,tk.customerUsername,tk.hallId,tk.seatsStr(),price.str(),status});
    }
    std::cout<<"\n";
    if(tickets.empty()) std::cout<<YELLOW<<"  No tickets yet.\n"<<RESET;
    else t.print();
    waitForEnter();
}

// ════════════════════════════════════════════════════════
//  RATING
// ════════════════════════════════════════════════════════
void MenuManager::rateMovie() {
    clearScreen(); printHeader("RATE A MOVIE");
    std::vector<Movie*> watched;
    for(auto& m:movies)
        for(auto& tk:tickets)
            if(tk.movieId==m.id&&tk.customerUsername==currentUser->username&&tk.status=="Confirmed")
                {watched.push_back(&m);break;}
    if(watched.empty()){std::cout<<YELLOW<<"\n  No confirmed bookings to rate.\n"<<RESET;waitForEnter();return;}
    tabulate::Table t; t.addHeader({"ID","Title","Your Rating"});
    for(auto* m:watched){
        std::string myR="Not rated";
        for(auto& r:m->ratings) if(r.username==currentUser->username){myR=Movie::ratingIcons(r.stars, false);break;}
        t.addRow({m->id,m->title,myR});
    }
    std::cout<<"\n"; t.print();
    std::string id=getStringInput("\n  Movie ID to rate: ");
    Movie* m=findMovie(id);
    if(!m){std::cout<<RED<<"  Not found!\n"<<RESET;waitForEnter();return;}
    for(auto& r:m->ratings){
        if(r.username==currentUser->username){
            std::cout<<YELLOW<<"  Already rated. Update? (y/n): "<<RESET;
            char c; std::cin>>c; std::cin.ignore();
            if(tolower(c)!='y'){waitForEnter();return;}
            m->ratings.erase(std::remove_if(m->ratings.begin(),m->ratings.end(),
                [&](const Rating& r){return r.username==currentUser->username;}),m->ratings.end());
            break;
        }
    }
    int stars=getIntInput("  Stars (1-5)",1,5);
    std::string comment=getStringInput("  Comment (optional): ");
    Rating r; r.username=currentUser->username; r.stars=stars;
    r.comment=comment.empty()?"--":comment; r.date=DataManager::todayDate();
    m->ratings.push_back(r);
    DataManager::saveRatings(movies);
    std::cout<<GREEN<<"\n  Review saved! "<<m->starsStr()<<"\n"<<RESET;
    waitForEnter();
}

// ════════════════════════════════════════════════════════
//  ADMIN
// ════════════════════════════════════════════════════════
void MenuManager::manageUsers() {
    clearScreen(); printHeader("USER MANAGEMENT");
    int c=chooseOptionBox("User Management", {
        {"1", "📋 List Users"},
        {"2", "➕ Add User"},
        {"3", "🗑️ Remove User"},
        {"0", "⬅️ Back"}
    }, "Choice", 0, 3);
    if(c==1){auth.listUsers();waitForEnter();}
    else if(c==2){
        std::string u=getStringInput("  Username: "),p=getStringInput("  Password: "),d=getStringInput("  Display Name: ");
        int r=chooseOptionBox("Role Options", {
            {"1", "🛠️ Admin"},
            {"2", "🍿 Customer"}
        }, "Role", 1, 2);
        auth.addUser(u,p,(r==1)?Role::ADMIN:Role::CUSTOMER,d);
        waitForEnter();
    } else if(c==3){
        auth.listUsers();
        auth.removeUser(getStringInput("  Username to remove: "));
        waitForEnter();
    }
}

void MenuManager::viewReports() {
    while (true) {
        clearScreen(); printHeader("REPORTS");

        // Always show summary at top
        double rev=0; int conf=0,canc=0;
        for(auto& tk:tickets){
            if(tk.status=="Confirmed"){rev+=tk.totalPrice;conf++;}
            else canc++;
        }
        tabulate::Table summary;
        summary.addHeader({"Metric", "Value"});
        summary.addRow({"Total Revenue", std::string(GREEN)+"$"+[&](){std::ostringstream o;o<<std::fixed<<std::setprecision(2)<<rev;return o.str();}()+RESET});
        summary.addRow({"Confirmed Tickets", std::string(GREEN)+std::to_string(conf)+RESET});
        summary.addRow({"Cancelled Tickets", std::string(RED)+std::to_string(canc)+RESET});
        summary.addRow({"Total Movies", std::string(CYAN)+std::to_string((int)movies.size())+RESET});
        summary.addRow({"Total Halls", std::string(CYAN)+std::to_string((int)halls.size())+RESET});
        std::cout<<"\n";
        summary.print();

        printSeparator();
        int choice = chooseOptionBox("Report Options", {
            {"1", "📊 All Movies Report"},
            {"2", "🎬 One Movie Detail"},
            {"3", "🏛️ Hall Summary"},
            {"4", "❌ Cancelled Tickets Detail"},
            {"0", "⬅️ Back"}
        }, "Choice", 0, 4);
        if (choice == 0) return;

        if (choice == 1) {
            // ── All movies ranked by revenue ─────────────
            clearScreen(); printHeader("ALL MOVIES REPORT");
            std::vector<Movie> sorted=movies;
            std::sort(sorted.begin(),sorted.end(),[](const Movie& a,const Movie& b){return a.bookedSeats>b.bookedSeats;});
            tabulate::Table t;
            t.addHeader({"Movie","Hall","Booked Seats","Revenue","Confirmed","Cancelled","Rating"});
            for(auto& m:sorted){
                double r=0; int mc=0,mx=0;
                for(auto& tk:tickets) if(tk.movieId==m.id){
                    if(tk.status=="Confirmed"){r+=tk.totalPrice;mc++;}
                    else mx++;
                }
                std::ostringstream rs; rs<<"$"<<std::fixed<<std::setprecision(2)<<r;
                Hall* h=findHall(m.hallId);
                t.addRow({m.title,h?h->name:m.hallId,std::to_string(m.bookedSeats),
                          rs.str(),std::to_string(mc),std::to_string(mx),m.starsStr()});
            }
            std::cout<<"\n"; t.print();
            waitForEnter();

        } else if (choice == 2) {
            // ── One movie detail ─────────────────────────
            clearScreen(); printHeader("MOVIE DETAIL REPORT");
            tabulate::Table ml; ml.addHeader({"ID","Title","Hall","Showtime"});
            for(auto& m:movies){Hall* h=findHall(m.hallId); ml.addRow({m.id,m.title,h?h->name:m.hallId,m.showtime});}
            std::cout<<"\n"; ml.print();
            std::string mid = getStringInput("\n  Enter Movie ID (or Enter to cancel): ");
            if (mid.empty()) continue;
            std::transform(mid.begin(),mid.end(),mid.begin(),::toupper);
            Movie* mv = findMovie(mid);
            if (!mv) { printCentered(RED+std::string("Movie not found!")+RESET); waitForEnter(); continue; }

            double mRev=0; int mConf=0,mCanc=0;
            std::vector<const Ticket*> bookedTickets;
            for(auto& tk:tickets) if(tk.movieId==mv->id){
                if(tk.status=="Confirmed"){mRev+=tk.totalPrice;mConf++;bookedTickets.push_back(&tk);}
                else mCanc++;
            }

            clearScreen(); printHeader("Movie: "+mv->title);
            Hall* mh=findHall(mv->hallId);
            std::cout<<"\n";
            std::ostringstream priceOut; priceOut<<"$"<<std::fixed<<std::setprecision(2)<<mv->price;
            std::ostringstream revenueOut; revenueOut<<"$"<<std::fixed<<std::setprecision(2)<<mRev;

            tabulate::Table detail;
            detail.addHeader({"Metric", "Value"});
            detail.addRow({"Hall", mh?mh->name:mv->hallId});
            detail.addRow({"Showtime", mv->showtime});
            detail.addRow({"Total Seats", std::to_string(mv->totalSeats)});
            detail.addRow({"Booked Seats", RED+std::to_string(mv->bookedSeats)+RESET});
            detail.addRow({"Available", GREEN+std::to_string(mv->availableSeats())+RESET});
            detail.addRow({"Ticket Price", priceOut.str()});
            detail.addRow({"Total Revenue", GREEN+revenueOut.str()+RESET});
            detail.addRow({"Confirmed", GREEN+std::to_string(mConf)+RESET});
            detail.addRow({"Cancelled", RED+std::to_string(mCanc)+RESET});
            detail.addRow({"Rating", mv->starsStr()});
            detail.print();

            if (!bookedTickets.empty()) {
                std::cout<<"\n";
                tabulate::Table booked;
                booked.addHeader({"No", "Customer", "Ticket", "Seat", "Date"});
                for(size_t i=0;i<bookedTickets.size();++i) {
                    const Ticket* tk = bookedTickets[i];
                    booked.addRow({std::to_string(i+1), tk->customerUsername, tk->ticketId, tk->seatsStr(), tk->bookingDate});
                }
                booked.print();
            }
            waitForEnter();

        } else if (choice == 3) {
            // ── Hall summary ─────────────────────────────
            clearScreen(); printHeader("HALL SUMMARY REPORT");
            tabulate::Table ht;
            ht.addHeader({"Hall","Total Seats","Booked","Available","Occupancy %","Revenue"});
            for(auto& h:halls){
                double hRev=0;
                for(auto& tk:tickets) if(tk.hallId==h.id&&tk.status=="Confirmed") hRev+=tk.totalPrice;
                int occ = h.totalSeats()>0 ? (h.bookedSeats()*100/h.totalSeats()) : 0;
                std::string occStr = occ>=80?(RED+std::to_string(occ)+"%"+RESET):
                                     occ>=50?(YELLOW+std::to_string(occ)+"%"+RESET):
                                     (GREEN+std::to_string(occ)+"%"+RESET);
                std::ostringstream rs; rs<<"$"<<std::fixed<<std::setprecision(2)<<hRev;
                ht.addRow({h.name,std::to_string(h.totalSeats()),
                           std::to_string(h.bookedSeats()),std::to_string(h.availableSeats()),
                           occStr,rs.str()});
            }
            std::cout<<"\n"; ht.print();

            // Ask for specific hall detail
            std::string hid = getStringInput("\n  Enter Hall ID for detail (or Enter to skip): ");
            if (!hid.empty()) {
                std::transform(hid.begin(),hid.end(),hid.begin(),::toupper);
                Hall* hl=findHall(hid);
                if (!hl) { printCentered(RED+std::string("Hall not found!")+RESET); }
                else {
                    clearScreen(); printHeader("Hall Detail: "+hl->name);
                    std::cout<<"\n";
                    // Show movies in this hall
                    tabulate::Table mt; mt.addHeader({"Movie","Showtime","Booked","Revenue","Status"});
                    for(auto& m:movies) if(m.hallId==hid){
                        double mr=0;
                        for(auto& tk:tickets) if(tk.movieId==m.id&&tk.status=="Confirmed") mr+=tk.totalPrice;
                        std::ostringstream rs; rs<<"$"<<std::fixed<<std::setprecision(2)<<mr;
                        std::string st=m.isSoldOut()?(std::string(RED)+"SOLD OUT"+RESET):(std::string(GREEN)+"Available"+RESET);
                        mt.addRow({m.title,m.showtime,std::to_string(m.bookedSeats),rs.str(),st});
                    }
                    mt.print();
                }
            }
            waitForEnter();

        } else if (choice == 4) {
            // ── Cancelled tickets detail ─────────────────
            clearScreen(); printHeader("CANCELLED TICKETS");
            tabulate::Table ct;
            ct.addHeader({"Ticket ID","Movie","Customer","Hall","Seats","Price","Date"});
            int count=0;
            for(auto& tk:tickets) if(tk.status=="Cancelled"){
                std::ostringstream p; p<<"$"<<std::fixed<<std::setprecision(2)<<tk.totalPrice;
                ct.addRow({tk.ticketId,tk.movieTitle,tk.customerUsername,
                           tk.hallId,tk.seatsStr(),p.str(),tk.bookingDate});
                count++;
            }
            std::cout<<"\n";
            if(count==0) printCentered(GREEN+std::string("No cancelled tickets!")+RESET);
            else {
                ct.print();
                std::cout<<"\n";
                printCentered(RED+std::string("Total Cancelled: ")+std::to_string(count)+RESET);
            }
            waitForEnter();
        }
    }
}

// ════════════════════════════════════════════════════════
//  HELPERS
// ════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════
//  NOW PLAYING — random movies based on timestamp
// ════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════
//  LIVE COUNTDOWN — ticks every second using a thread
//  Press Enter to stop
// ════════════════════════════════════════════════════════
void MenuManager::showStartingSoon() {
    struct DashMovie {
        const Movie* movie;
        std::string hallName;
        std::string priceStr;
        std::time_t movieTime;
        int available;
    };

    std::vector<DashMovie> upcoming;
    int availableSeats = 0;
    for (const auto& mv : movies) {
        availableSeats += std::max(0, mv.availableSeats());
        std::time_t movieTime = 0;
        if (!parseShowtime(mv.showtime, movieTime)) continue;
        if (std::difftime(movieTime, std::time(nullptr)) < -60) continue;
        Hall* hl = findHall(mv.hallId);
        std::ostringstream price;
        price << "$" << std::fixed << std::setprecision(2) << mv.price;
        upcoming.push_back({&mv, hl ? hl->name : mv.hallId, price.str(), movieTime, mv.availableSeats()});
    }

    std::sort(upcoming.begin(), upcoming.end(), [](const DashMovie& a, const DashMovie& b) {
        return a.movieTime < b.movieTime;
    });

    auto countdownText = [](std::time_t movieTime, std::time_t now) {
        long total = static_cast<long>(std::max(0.0, std::difftime(movieTime, now)));
        int h = static_cast<int>(total / 3600);
        int m = static_cast<int>((total % 3600) / 60);
        int s = static_cast<int>(total % 60);
        std::ostringstream out;
        out << std::setw(2) << std::setfill('0') << h << ":"
            << std::setw(2) << std::setfill('0') << m << ":"
            << std::setw(2) << std::setfill('0') << s;
        return out.str();
    };

    auto visibleTitle = [](std::string text, int maxLen) {
        if (visLen(text) <= maxLen) return text;
        while (!text.empty() && visLen(text + "...") > maxLen) text.pop_back();
        return text + "...";
    };

    auto renderDashboard = [&]() {
        const int dashW = 106;
        const int contentW = dashW - 2;
        const int panelW = 34;
        const int panelTotalW = 3 * panelW + 4;
        const std::time_t now = std::time(nullptr);
        std::tm* nowTm = std::localtime(&now);
        char dateBuf[32], timeBuf[16];
        std::strftime(dateBuf, sizeof(dateBuf), "%b %d, %Y", nowTm);
        std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", nowTm);

        std::string nextMovie = upcoming.empty() ? "No upcoming show" : visibleTitle(upcoming.front().movie->title, 16);
        std::string nextCountdown = upcoming.empty() ? "--:--:--" : countdownText(upcoming.front().movieTime, now);
        const DashMovie* rec = upcoming.empty() ? nullptr : &upcoming.front();

        auto fullBorder = [&](const std::string& left, const std::string& right) {
            return std::string(B2) + BOLD + left + repeat("─", contentW) + right + RESET;
        };
        auto fullRow = [&](const std::string& text) {
            return std::string(B2) + BOLD + "│" + RESET + centerIn(text, contentW)
                 + std::string(B2) + BOLD + "│" + RESET;
        };
        auto panelLine = [&](const std::string& a, const std::string& b, const std::string& c) {
            return std::string(B2) + BOLD + "│" + RESET + centerIn(a, panelW)
                 + std::string(B2) + BOLD + "│" + RESET + centerIn(b, panelW)
                 + std::string(B2) + BOLD + "│" + RESET + centerIn(c, panelW)
                 + std::string(B2) + BOLD + "│" + RESET;
        };

        std::vector<int> widths = {4, 24, 14, 11, 27, 10, 12};
        int tableW = 1;
        for (int w : widths) tableW += w + 1;
        auto tableBorder = [&](const std::string& left, const std::string& mid, const std::string& right) {
            std::ostringstream out;
            out << B2 << BOLD << left;
            for (size_t i = 0; i < widths.size(); ++i) {
                out << repeat("─", widths[i]) << (i + 1 == widths.size() ? right : mid);
            }
            out << RESET;
            return out.str();
        };
        auto tableRow = [&](const std::vector<std::string>& cells, bool header) {
            std::ostringstream out;
            out << B2 << BOLD << "│" << RESET;
            for (size_t i = 0; i < widths.size(); ++i) {
                std::string cell = i < cells.size() ? cells[i] : "";
                if (header) out << B3 << BOLD << centerIn(cell, widths[i]) << RESET;
                else        out << " " << padRightVisible(cell, widths[i] - 1);
                out << B2 << BOLD << "│" << RESET;
            }
            return out.str();
        };

        std::vector<std::string> lines;
        lines.push_back("");
        lines.push_back(indentFor(dashW) + fullBorder("┌", "┐"));
        lines.push_back(indentFor(dashW) + fullRow(std::string(B3) + BOLD + "☆  ☆  ═══  🎞️  MOVIE4Q4  🎬  ═══  ☆  ☆" + RESET));
        lines.push_back(indentFor(dashW) + fullRow(std::string(B5) + BOLD + "MOVIE TICKET SYSTEM" + RESET));
        lines.push_back(indentFor(dashW) + fullRow(std::string(B3) + "BOOK  |  WATCH  |  ENJOY" + RESET));
        lines.push_back(indentFor(dashW) + fullBorder("└", "┘"));
        lines.push_back("");
        lines.push_back(indentFor(panelTotalW) + std::string(B2) + BOLD + "┌" + repeat("─", panelW) + "┬" + repeat("─", panelW) + "┬" + repeat("─", panelW) + "┐" + RESET);
        lines.push_back(indentFor(panelTotalW) + panelLine("📅 CURRENT DATE & TIME", "⏳ NEXT SHOW COUNTDOWN", "🎟️ TODAY'S SUMMARY"));
        lines.push_back(indentFor(panelTotalW) + panelLine(repeat("─", 26), repeat("─", 26), repeat("─", 26)));
        lines.push_back(indentFor(panelTotalW) + panelLine(std::string("Date : ") + B5 + dateBuf + RESET,
                                                          std::string("Next Show : ") + B4 + nextMovie + RESET,
                                                          std::string("Total Movies : ") + GREEN + std::to_string(movies.size()) + RESET));
        lines.push_back(indentFor(panelTotalW) + panelLine(std::string("Time : ") + GREEN + timeBuf + RESET,
                                                          std::string("Starts in : ") + "\033[38;2;148;0;211m" + nextCountdown + RESET,
                                                          std::string("Available Seats : ") + GREEN + std::to_string(availableSeats) + RESET));
        lines.push_back(indentFor(panelTotalW) + std::string(B2) + BOLD + "└" + repeat("─", panelW) + "┴" + repeat("─", panelW) + "┴" + repeat("─", panelW) + "┘" + RESET);
        lines.push_back("");
        lines.push_back(indentFor(tableW) + tableBorder("┌", "┬", "┐"));
        lines.push_back(indentFor(tableW) + tableRow({"No", "🎬 Movie", "🏛️ Hall", "Show Time", "Countdown", "Price", "Status"}, true));
        lines.push_back(indentFor(tableW) + tableBorder("├", "┼", "┤"));

        const int maxRows = 6;
        for (int i = 0; i < maxRows; ++i) {
            if (i < static_cast<int>(upcoming.size())) {
                const DashMovie& dm = upcoming[i];
                std::string clock = dm.movie->showtime.size() >= 16 ? dm.movie->showtime.substr(11, 5) : dm.movie->showtime;
                std::string cd = "Starts in " + std::string("\033[38;2;148;0;211m") + countdownText(dm.movieTime, now) + RESET;
                std::string status = dm.available <= 0 ? RED + std::string("● Sold Out") + RESET :
                                     dm.available <= 5 ? "\033[38;2;148;0;211m● Almost Full\033[0m" :
                                     GREEN + std::string("● Available") + RESET;
                lines.push_back(indentFor(tableW) + tableRow({std::to_string(i + 1),
                                                              "🎬 " + visibleTitle(dm.movie->title, 20),
                                                              dm.hallName,
                                                              clock,
                                                              cd,
                                                              dm.priceStr,
                                                              status}, false));
            } else {
                lines.push_back(indentFor(tableW) + tableRow({"", DIM + std::string("No more upcoming movies") + RESET, "", "", "", "", ""}, false));
            }
            lines.push_back(indentFor(tableW) + (i + 1 == maxRows ? tableBorder("└", "┴", "┘")
                                                                  : tableBorder("├", "┼", "┤")));
        }

        lines.push_back("");
        lines.push_back(indentFor(dashW) + fullBorder("┌", "┐"));
        if (rec) {
            std::ostringstream rating;
            if (rec->movie->averageRating() > 0)
                rating << std::fixed << std::setprecision(1) << rec->movie->averageRating() << "/5";
            else
                rating << "New pick";
            lines.push_back(indentFor(dashW) + fullRow(std::string(B3) + BOLD + "💡 RECOMMENDATION FOR YOU" + RESET));
            lines.push_back(indentFor(dashW) + fullRow(std::string(B4) + "Based on show time, we recommend: " + RESET +
                                                       GREEN + BOLD + rec->movie->title + RESET));
            lines.push_back(indentFor(dashW) + fullRow(std::string(B5) + "Genre: " + rec->movie->genre + "  |  Rating: " + rating.str() +
                                                       "  |  Enjoy your movie time ✨" + RESET));
        } else {
            lines.push_back(indentFor(dashW) + fullRow(std::string(YELLOW) + "No upcoming recommendations yet." + RESET));
            lines.push_back(indentFor(dashW) + fullRow(std::string(B5) + "Add a future movie showtime to fill this dashboard." + RESET));
            lines.push_back(indentFor(dashW) + fullRow(""));
        }
        lines.push_back(indentFor(dashW) + fullBorder("└", "┘"));
        lines.push_back(center(std::string(DIM) + "(Press Enter to continue)" + RESET));
        return lines;
    };

    const bool interactive = isatty(STDIN_FILENO);
    std::atomic<bool> running(true);
    if (interactive) {
        std::cout << "\033[?25l";
        std::thread inputThread([&running]() {
            std::cin.get();
            running = false;
        });
        inputThread.detach();
    }

    bool firstDraw = true;
    int totalLines = 0;
    do {
        if (!firstDraw) std::cout << "\033[" << totalLines << "A\033[J";
        firstDraw = false;
        std::vector<std::string> lines = renderDashboard();
        for (const auto& line : lines) std::cout << line << "\n";
        totalLines = static_cast<int>(lines.size());
        std::cout.flush();
        if (!interactive) break;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (running);

    if (interactive) std::cout << "\033[?25h\n";
}

void MenuManager::showNowPlaying() {
    if (movies.empty()) return;

    clearScreen();

    // Get current time info for display
    time_t now = time(0);
    tm* t = localtime(&now);
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%A, %B %d %Y  %H:%M", t);

    const int titleBoxW = 52;
    const std::string titleBoxI = indentFor(titleBoxW + 2);
    std::cout << CYAN << BOLD;
    std::cout << "\n" << titleBoxI << "┌" << repeat("─", titleBoxW) << "┐\n";
    std::cout << titleBoxI << "│" << centerIn("MOVIE TICKET MANAGEMENT SYSTEM", titleBoxW) << "│\n";
    std::cout << titleBoxI << "└" << repeat("─", titleBoxW) << "┘\n" << RESET;
    printCentered(std::string(DIM) + "Today: " + timeStr + RESET);
    std::cout << "\n";
    printCentered(std::string(BOLD) + GREEN + "NOW SHOWING TODAY" + RESET);
    std::cout << "\n";

    // Get today's date for filtering
    char todayBuf[11]; strftime(todayBuf, sizeof(todayBuf), "%Y-%m-%d", t);
    std::string todayStr(todayBuf);

    // Only pick movies showing today or in the future
    std::vector<int> indices;
    for (int i = 0; i < (int)movies.size(); i++) {
        std::string movieDate = movies[i].showtime.size()>=10 ? movies[i].showtime.substr(0,10) : "";
        if (movieDate >= todayStr) indices.push_back(i);
    }
    if (indices.empty()) {
        // fallback: show all if none are current
        for (int i = 0; i < (int)movies.size(); i++) indices.push_back(i);
    }

    // Fisher-Yates shuffle
    for (int i = (int)indices.size()-1; i > 0; i--) {
        int j = rand() % (i+1);
        std::swap(indices[i], indices[j]);
    }

    int showCount = std::min(3, (int)movies.size());

    tabulate::Table tbl;
    tbl.addHeader({"#", "Movie", "Hall", "Showtime", "Available", "Price", "Rating"});

    for (int i = 0; i < showCount; i++) {
        const Movie& m = movies[indices[i]];
        Hall* h = findHall(m.hallId);
        std::string hallName = h ? h->name : m.hallId;
        std::ostringstream price;
        price << "$" << std::fixed << std::setprecision(2) << m.price;
        std::string avail = m.isSoldOut() ?
            (RED + std::string("SOLD OUT") + RESET) :
            (GREEN + std::to_string(m.availableSeats()) + RESET);
        tbl.addRow({std::to_string(i+1), m.title, hallName,
                    m.showtime, avail, price.str(), m.starsStr()});
    }
    tbl.print();

    // Show starting soon
    showStartingSoon();
    printCentered(std::string(DIM) + "Refreshes each time you run the program." + RESET);
    std::cout << "\n";
    printCentered(BOLD + std::string("Press Enter to continue...") + RESET);
    std::cin.get();
}

void MenuManager::printBanner() {
    clearScreen();
    const int innerW = 68;
    const int fullW = innerW + 2;
    const std::string I = indentFor(fullW);
    auto border = [&](const std::string& left, const std::string& right) {
        std::cout << B2 << BOLD << I << left << repeat("─", innerW) << right << "\n" << RESET;
    };
    auto row = [&](const std::string& text, const char* color, bool bold = true) {
        std::cout << B2 << BOLD << I << "│" << RESET
                  << color << (bold ? BOLD : "") << centerIn(text, innerW) << RESET
                  << B2 << BOLD << "│\n" << RESET;
    };

    std::cout << "\n";
    border("┌", "┐");
    for (const auto& artLine : ticketBannerLines("MOVIE4Q4")) {
        row(artLine, "", false);
    }
    row("", B3);
    row("🎥 Lights, camera, action! Welcome to Movie4Q4 ✨", B5, false);
    row("🎥 Welcome user! Time to book your favorite movie 🎬", B5, false);
    border("└", "┘");
    std::cout << "\n";
    border("┌", "┐");
    row("Login / Register  |  data stored in data/users.xlsx", B5, false);
    border("└", "┘");
    std::cout << "\n";
}
void MenuManager::printHeader(const std::string& title) {
    int full = std::min(72, std::max(54, terminalWidth() - 8));
    full = std::max(full, visLen(title) + 4);
    std::string bar(full, '=');
    std::string I = indentFor(full);
    std::cout << "\n";
    std::cout << B2 << BOLD << I << bar << RESET << "\n";
    std::cout << B3 << BOLD << I << centerIn(title, full) << RESET << "\n";
    std::cout << B2 << BOLD << I << bar << RESET << "\n";
}
void MenuManager::printSeparator() {
    int full = std::min(72, std::max(54, terminalWidth() - 8));
    std::cout << B2 << indentFor(full) << repeat("─", full) << RESET << "\n";
}

void MenuManager::showLoading(const std::string& title, const std::string& subtitle) {
    clearScreen();
    printHeader(title);
    const int barW = 42;
    const int fullW = barW + 14;
    std::cout << "\n";
    for (int progress = 0; progress <= 100; progress += 4) {
        int filled = (progress * barW) / 100;
        std::string bar = repeat("█", filled) + repeat("░", barW - filled);
        std::ostringstream line;
        line << B2 << "[" << RESET
             << GREEN << bar << RESET
             << B2 << "] " << RESET
             << BOLD << std::setw(3) << progress << "%" << RESET;
        std::cout << "\r" << indentFor(fullW) << line.str() << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(28));
    }
    std::cout << "\n\n";
    printCentered(std::string(B4) + BOLD + subtitle + RESET);
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
}

void MenuManager::showInlineLoading(const std::string& title, const std::string& subtitle) {
    printHeader(title);
    const int barW = 42;
    const int fullW = barW + 14;
    std::cout << "\n";
    for (int progress = 0; progress <= 100; progress += 4) {
        int filled = (progress * barW) / 100;
        std::string bar = repeat("█", filled) + repeat("░", barW - filled);
        std::ostringstream line;
        line << B2 << "[" << RESET
             << GREEN << bar << RESET
             << B2 << "] " << RESET
             << BOLD << std::setw(3) << progress << "%" << RESET;
        std::cout << "\r" << indentFor(fullW) << line.str() << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(28));
    }
    std::cout << "\n\n";
    printCentered(std::string(B4) + BOLD + subtitle + RESET);
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
}

int MenuManager::getIntInput(const std::string& prompt, int min, int max) {
    if (isatty(STDIN_FILENO)) {
        std::string cleanPrompt = ltrimPrompt(prompt);
        std::string plain = ">> " + cleanPrompt + " (" + std::to_string(min) + "-" + std::to_string(max) + "): ";
        int selected = min;
        std::string typed;

        termios oldTerm {};
        tcgetattr(STDIN_FILENO, &oldTerm);
        termios raw = oldTerm;
        raw.c_lflag &= static_cast<unsigned long>(~(ICANON | ECHO));
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);

        auto restore = [&]() {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldTerm);
        };
        auto render = [&]() {
            std::string value = typed.empty()
                ? (std::string(BOLD) + std::to_string(selected) + RESET)
                : (std::string(BOLD) + typed + RESET);
            std::cout << "\r\033[K" << indentFor(visLen(plain + value))
                      << B3 << ">> " << RESET << cleanPrompt
                      << " (" << min << "-" << max << "): " << value << std::flush;
        };

        std::cout << "\n";
        render();
        while (true) {
            char ch = 0;
            if (read(STDIN_FILENO, &ch, 1) != 1) continue;
            if (ch == '\n' || ch == '\r') {
                restore();
                std::cout << "\n";
                if (!typed.empty()) {
                    try {
                        int val = std::stoi(typed);
                        if (val >= min && val <= max) return val;
                    } catch (...) {}
                    printCentered(RED + std::string("Enter a number between ") +
                                  std::to_string(min) + " and " + std::to_string(max) + "." + RESET);
                    typed.clear();
                    return getIntInput(prompt, min, max);
                }
                return selected;
            }
            if (ch == 27) {
                char seq[2] = {0, 0};
                if (read(STDIN_FILENO, &seq[0], 1) == 1 &&
                    read(STDIN_FILENO, &seq[1], 1) == 1 && seq[0] == '[') {
                    typed.clear();
                    if (seq[1] == 'A') selected = (selected <= min) ? max : selected - 1;
                    if (seq[1] == 'B') selected = (selected >= max) ? min : selected + 1;
                    render();
                }
            } else if (std::isdigit(static_cast<unsigned char>(ch))) {
                typed += ch;
                render();
            } else if ((ch == 127 || ch == 8) && !typed.empty()) {
                typed.pop_back();
                render();
            }
        }
    }

    int val;
    while(true) {
        if(!prompt.empty()) {
            std::string cleanPrompt = ltrimPrompt(prompt);
            std::string plain = ">> " + cleanPrompt + " (" + std::to_string(min) + "-" + std::to_string(max) + "): ";
            std::cout << "\n" << indentFor(visLen(plain))
                      << B3 << ">> " << RESET << cleanPrompt << " (" << min << "-" << max << "): ";
        }
        if(std::cin >> val && val >= min && val <= max) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return val;
        }
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        printCentered(RED + std::string("Enter a number between ") +
                      std::to_string(min) + " and " + std::to_string(max) + "." + RESET);
    }
}
int MenuManager::getValidInt(const std::string& prompt){
    int val;
    while(true){
        printInputPrompt(prompt);
        if(std::cin>>val&&val>0){std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');return val;}
        std::cin.clear();std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
        printCentered(RED+std::string("Positive number please.")+RESET);
    }
}
double MenuManager::getDoubleInput(const std::string& prompt){
    double val;
    while(true){
        printInputPrompt(prompt);
        if(std::cin>>val&&val>=0){std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');return val;}
        std::cin.clear();std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
        printCentered(RED+std::string("Invalid.")+RESET);
    }
}
std::string MenuManager::getStringInput(const std::string& prompt){
    printInputPrompt(prompt);std::string in;std::getline(std::cin,in);return in;
}
void MenuManager::waitForEnter(){
    std::string prompt = "Press Enter to continue...";
    std::cout << DIM << "\n" << indentFor(visLen(prompt)) << prompt << RESET;
    std::cin.get();
}
void MenuManager::clearScreen(){std::cout<<"\033[2J\033[H";}
Movie*  MenuManager::findMovie(const std::string& id){for(auto& m:movies) if(m.id==id) return &m;return nullptr;}
Ticket* MenuManager::findTicket(const std::string& id){for(auto& t:tickets) if(t.ticketId==id) return &t;return nullptr;}
Hall*   MenuManager::findHall(const std::string& id){for(auto& h:halls) if(h.id==id) return &h;return nullptr;}
void MenuManager::saveAll(){
    DataManager::saveMovies(movies);
    DataManager::saveTickets(tickets);
    DataManager::saveRatings(movies);
    DataManager::saveHalls(halls);
}
