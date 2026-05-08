#include "User.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <clocale>
#include <cwchar>
#include <sys/ioctl.h>
#include <unistd.h>

// Ocean Blue theme
#define RESET  "\033[0m"
#define BOLD   "\033[1m"
#define B2     "\033[38;2;0;119;182m"    // border color
#define B3     "\033[38;2;0;180;216m"    // title color
#define B4     "\033[38;2;144;224;239m"  // default text

// Row colors — cycles through these for each option
static const char* ROW_COLORS[] = {
    "\033[38;2;199;21;133m",   // dark pink
    "\033[38;2;128;0;128m",    // purple
    "\033[38;2;88;187;67m",    // green
    "\033[38;2;220;20;60m",    // crimson
    "\033[38;2;0;92;255m",     // blue
    "\033[38;2;0;38;120m",     // dark blue
    "\033[38;2;0;180;216m",    // sky blue
    "\033[38;2;148;0;211m",    // violet
    "\033[38;2;176;48;96m",    // deep pink
};
static const int NUM_COLORS = 9;

static const int NO_W = 6;
static const int OPT_W = 48;

static int visibleLen(const std::string& s) {
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

static std::string padRight(const std::string& text, int width) {
    int pad = width - visibleLen(text);
    return text + std::string(std::max(0, pad), ' ');
}

static std::string centerIn(const std::string& text, int width) {
    int pad = width - visibleLen(text);
    int left = std::max(0, pad / 2);
    int right = std::max(0, pad - left);
    return std::string(left, ' ') + text + std::string(right, ' ');
}

static std::string repeat(const std::string& text, int count) {
    std::string out;
    for (int i = 0; i < count; ++i) out += text;
    return out;
}

static int tableWidth() {
    return NO_W + OPT_W + 3;
}

static void border(const std::string& left, const std::string& mid, const std::string& right) {
    std::cout << B2 << indentFor(tableWidth())
              << left << repeat("─", NO_W) << mid << repeat("─", OPT_W) << right
              << RESET << "\n";
}

static void spanBorder(const std::string& left, const std::string& right) {
    std::cout << B2 << indentFor(tableWidth())
              << left << repeat("─", NO_W + 1 + OPT_W) << right
              << RESET << "\n";
}

static void titleRow(const std::string& t) {
    std::cout << B2 << indentFor(tableWidth()) << "│" << RESET
              << B3 << BOLD << centerIn(t, NO_W + 1 + OPT_W)
              << RESET << B2 << "│" << RESET << "\n";
}

static void headerRow() {
    std::cout << B2 << indentFor(tableWidth()) << "│" << RESET
              << B3 << BOLD << centerIn("No", NO_W) << RESET
              << B2 << "│" << RESET
              << B3 << BOLD << " " << padRight("Options", OPT_W - 1) << RESET
              << B2 << "│" << RESET << "\n";
}

static void optRow(const std::string& num, const std::string& label, int colorIdx) {
    const char* col = ROW_COLORS[colorIdx % NUM_COLORS];
    std::cout << B2 << indentFor(tableWidth()) << "│" << RESET
              << B3 << BOLD << centerIn(num, NO_W) << RESET
              << B2 << "│" << RESET
              << col << BOLD << " " << padRight(label, OPT_W - 1) << RESET
              << B2 << "│" << RESET << "\n";
}

static void optionTable(const std::string& title,
                        const std::vector<std::pair<std::string, std::string>>& options) {
    std::cout << "\n";
    spanBorder("┌", "┐");
    titleRow(title);
    border("├", "┬", "┤");
    headerRow();
    border("├", "┼", "┤");
    for (size_t i = 0; i < options.size(); ++i) {
        optRow(options[i].first, options[i].second, static_cast<int>(i));
        if (i + 1 == options.size()) {
            border("└", "┴", "┘");
        } else {
            border("├", "┼", "┤");
        }
    }
    std::cout << "\n";
}

void Admin::showMenu() {
    optionTable("Admin Options", {
        {"1", "🎬 Manage Movies"},
        {"2", "🏛️ Manage Halls"},
        {"3", "🎟️ View All Tickets"},
        {"4", "👤 Manage Users"},
        {"5", "📊 Reports"},
        {"6", "🔍 Search Movies"},
        {"0", "🚪 Logout"}
    });
}

void Staff::showMenu() {
    optionTable("Staff Options", {
        {"1", "🎬 View Movies"},
        {"2", "➕ Add Movie"},
        {"3", "✏️ Edit Movie"},
        {"4", "🎟️ View All Tickets"},
        {"5", "🏛️ View Halls"},
        {"0", "🚪 Logout"}
    });
}

void Customer::showMenu() {
    optionTable("Customer Options", {
        {"1", "🍿 Browse Movies"},
        {"2", "⭐ Movie Details & Reviews"},
        {"3", "🪑 View by Hall & Seat Map"},
        {"4", "🎟️ Book Ticket"},
        {"5", "🎫 My Tickets"},
        {"6", "❌ Cancel Ticket"},
        {"7", "💬 Rate a Movie"},
        {"8", "🕘 Booking History"},
        {"9", "🔍 Search Movies"},
        {"0", "🚪 Logout"}
    });
}
