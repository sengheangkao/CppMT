#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <clocale>
#include <cwchar>
#include <sys/ioctl.h>
#include <unistd.h>

namespace tabulate {

static size_t visibleLen(const std::string& s) {
    static const bool localeReady = []() {
        std::setlocale(LC_CTYPE, "");
        return true;
    }();
    (void)localeReady;

    size_t len = 0;
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
            len += static_cast<size_t>(width);
        } else {
            state = std::mbstate_t {};
        }
        i += used;
    }
    return len;
}

static std::string padRight(const std::string& s, size_t width) {
    size_t vl=visibleLen(s);
    return s+std::string(width>vl?width-vl:0,' ');
}

static std::string repeat(const std::string& text, size_t count) {
    std::string out;
    for (size_t i = 0; i < count; ++i) out += text;
    return out;
}

static int terminalWidth() {
    struct winsize w {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return static_cast<int>(w.ws_col);
    }
    return 100;
}

static std::string indentFor(size_t visibleWidth) {
    int pad = (terminalWidth() - static_cast<int>(visibleWidth)) / 2;
    return std::string(std::max(0, pad), ' ');
}

class Table {
public:
    using Row=std::vector<std::string>;

    void addHeader(const Row& row){headers=row;updateWidths(row);}
    void addRow(const Row& row)   {rows.push_back(row);updateWidths(row);}
    void clear(){rows.clear();headers.clear();colWidths.clear();}

    void print() const {
        if(headers.empty()&&rows.empty()){std::cout<<indentFor(7)<<"(empty)\n";return;}
        // Colors
        std::string DPINK="\033[38;2;0;119;182m";
        std::string MPINK="\033[38;2;246;38;129m";
        std::string RST  ="\033[0m";

        const std::string I = indentFor(width());

        printBorder("┌","─","┬","┐",DPINK,RST,I);
        if(!headers.empty()){
            printRow(headers,true,DPINK,MPINK,RST,I);
            printBorder("├","─","┼","┤",DPINK,RST,I);
        }
        for(size_t i=0;i<rows.size();i++){
            printRow(rows[i],false,DPINK,MPINK,RST,I);
            if(i<rows.size()-1)
                printBorder("├","─","┼","┤",DPINK,RST,I);
        }
        printBorder("└","─","┴","┘",DPINK,RST,I);
        std::cout<<DPINK<<I<<rows.size()<<" row(s)"<<RST<<"\n";
    }

private:
    Row headers;
    std::vector<Row> rows;
    std::vector<size_t> colWidths;

    void updateWidths(const Row& row){
        if(colWidths.size()<row.size()) colWidths.resize(row.size(),0);
        for(size_t i=0;i<row.size();i++)
            colWidths[i]=std::max(colWidths[i],visibleLen(row[i])+2);
    }

    size_t width() const {
        size_t total = 1;
        for (auto colWidth : colWidths) total += colWidth + 1;
        return total;
    }

    // All border chars passed as strings — no char type used
    void printBorder(const std::string& left, const std::string& fill,
                     const std::string& mid,  const std::string& right,
                     const std::string& color,const std::string& rst,
                     const std::string& indent) const {
        std::cout<<color<<indent<<left;
        for(size_t i=0;i<colWidths.size();i++){
            std::cout<<repeat(fill, colWidths[i]);
            std::cout<<(i+1<colWidths.size()?mid:right);
        }
        std::cout<<rst<<"\n";
    }

    void printRow(const Row& row,bool isHeader,
                  const std::string& DPINK,const std::string& MPINK,
                  const std::string& RST,const std::string& indent) const {
        std::cout<<indent<<DPINK<<"│"<<RST;
        for(size_t i=0;i<colWidths.size();i++){
            std::string cell=(i<row.size())?row[i]:"";
            std::string display=isHeader?(MPINK+"\033[1m"+cell+RST):cell;
            size_t w=colWidths[i];
            if(isHeader){
                size_t vl=visibleLen(cell);
                size_t pad=(w>vl)?(w-vl)/2:0;
                size_t rpad=(w>vl)?w-vl-pad:0;
                std::cout<<std::string(pad,' ')<<display<<std::string(rpad,' ');
            } else {
                std::cout<<" "<<padRight(display,w-1);
            }
            std::cout<<DPINK<<"│"<<RST;
        }
        std::cout<<"\n";
    }
};

} // namespace tabulate
