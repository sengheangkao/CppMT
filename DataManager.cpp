#include "DataManager.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <iostream>
#include <mach-o/dyld.h>
#include <limits.h>

static std::string getExeDir() {
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        std::filesystem::path p(path);
        return p.parent_path().string();
    }
    return ".";
}

const std::string DataManager::MOVIES_FILE   = getExeDir() + "/data/movies.xlsx";
const std::string DataManager::TICKETS_FILE  = getExeDir() + "/data/tickets.xlsx";
const std::string DataManager::USERS_FILE    = getExeDir() + "/data/users.xlsx";
const std::string DataManager::RATINGS_FILE  = getExeDir() + "/data/ratings.xlsx";
const std::string DataManager::HALLS_FILE    = getExeDir() + "/data/halls.xlsx";

static void saveWorkbook(xlnt::workbook& wb, const std::string& path) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    const std::string tmp = path + ".tmp.xlsx";
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
    wb.save(tmp);
    std::filesystem::remove(path, ec);
    std::filesystem::rename(tmp, path);
}

std::string DataManager::cellStr(xlnt::worksheet& ws, int row, int col) {
    try { return ws.cell(col, row).to_string(); } catch (...) { return ""; }
}
std::string DataManager::generateId(const std::string& prefix, int count) {
    std::ostringstream oss;
    oss << prefix << std::setw(4) << std::setfill('0') << (count + 1);
    return oss.str();
}
std::string DataManager::todayDate() {
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    std::ostringstream oss;
    oss << (tm->tm_year+1900) << "-"
        << std::setw(2) << std::setfill('0') << (tm->tm_mon+1) << "-"
        << std::setw(2) << std::setfill('0') << tm->tm_mday;
    return oss.str();
}

// ════════════════════════════════════════════════════════
//  HALLS  — saves hall structure + which seats are booked
// ════════════════════════════════════════════════════════
void DataManager::saveHalls(const std::vector<Hall>& halls) {
    std::filesystem::create_directories(getExeDir() + "/data");
    xlnt::workbook wb;

    // Sheet 1: hall definitions
    auto ws = wb.active_sheet();
    ws.title("Halls");
    ws.cell(1,1).value("ID");
    ws.cell(2,1).value("Name");
    ws.cell(3,1).value("Rows");
    ws.cell(4,1).value("Cols");
    for (int i = 0; i < (int)halls.size(); i++) {
        int r = i+2;
        ws.cell(1,r).value(halls[i].id);
        ws.cell(2,r).value(halls[i].name);
        ws.cell(3,r).value(halls[i].rows);
        ws.cell(4,r).value(halls[i].cols);
    }

    // Sheet 2: booked seats
    auto ws2 = wb.create_sheet();
    ws2.title("BookedSeats");
    ws2.cell(1,1).value("HallID");
    ws2.cell(2,1).value("SeatID");
    ws2.cell(3,1).value("BookedBy");
    int r = 2;
    for (auto& h : halls)
        for (auto& row : h.seats)
            for (auto& s : row)
                if (s.booked) {
                    ws2.cell(1,r).value(h.id);
                    ws2.cell(2,r).value(s.seatId);
                    ws2.cell(3,r).value(s.bookedBy);
                    r++;
                }

    saveWorkbook(wb, HALLS_FILE);
}

std::vector<Hall> DataManager::loadHalls() {
    std::vector<Hall> halls;
    if (!std::filesystem::exists(HALLS_FILE)) return halls;
    try {
        xlnt::workbook wb; wb.load(HALLS_FILE);

        // Load hall definitions
        auto ws = wb.sheet_by_title("Halls");
        int rows = ws.highest_row();
        for (int r = 2; r <= rows; r++) {
            std::string id   = cellStr(ws,r,1);
            std::string name = cellStr(ws,r,2);
            int hrows = std::stoi(cellStr(ws,r,3).empty()?"0":cellStr(ws,r,3));
            int hcols  = std::stoi(cellStr(ws,r,4).empty()?"0":cellStr(ws,r,4));
            if (!id.empty() && hrows>0 && hcols>0)
                halls.push_back(Hall(id, name, hrows, hcols));
        }

        // Load booked seats
        auto ws2 = wb.sheet_by_title("BookedSeats");
        int brows = ws2.highest_row();
        for (int r = 2; r <= brows; r++) {
            std::string hid    = cellStr(ws2,r,1);
            std::string seatId = cellStr(ws2,r,2);
            std::string by     = cellStr(ws2,r,3);
            if (hid.empty()) continue;
            for (auto& h : halls)
                if (h.id == hid) { h.bookSeat(seatId, by); break; }
        }
    } catch (const std::exception& e) {
        std::cerr << "[DataManager] Error loading halls: " << e.what() << "\n";
    }
    return halls;
}

// ════════════════════════════════════════════════════════
//  MOVIES
// ════════════════════════════════════════════════════════
void DataManager::saveMovies(const std::vector<Movie>& movies) {
    std::filesystem::create_directories(getExeDir() + "/data");
    xlnt::workbook wb; auto ws = wb.active_sheet(); ws.title("Movies");
    ws.cell(1,1).value("ID");       ws.cell(2,1).value("Title");
    ws.cell(3,1).value("Genre");    ws.cell(4,1).value("Showtime");
    ws.cell(5,1).value("TotalSeats"); ws.cell(6,1).value("BookedSeats");
    ws.cell(7,1).value("Price");    ws.cell(8,1).value("Description");
    ws.cell(9,1).value("HallID");
    for (int i = 0; i < (int)movies.size(); i++) {
        int r = i+2; const Movie& m = movies[i];
        ws.cell(1,r).value(m.id);          ws.cell(2,r).value(m.title);
        ws.cell(3,r).value(m.genre);       ws.cell(4,r).value(m.showtime);
        ws.cell(5,r).value(m.totalSeats);  ws.cell(6,r).value(m.bookedSeats);
        ws.cell(7,r).value(m.price);       ws.cell(8,r).value(m.description);
        ws.cell(9,r).value(m.hallId);
    }
    saveWorkbook(wb, MOVIES_FILE);
}

std::vector<Movie> DataManager::loadMovies() {
    std::vector<Movie> movies;
    if (!std::filesystem::exists(MOVIES_FILE)) return movies;
    try {
        xlnt::workbook wb; wb.load(MOVIES_FILE);
        auto ws = wb.active_sheet();
        int rows = ws.highest_row();
        for (int r = 2; r <= rows; r++) {
            Movie m;
            m.id          = cellStr(ws,r,1); m.title    = cellStr(ws,r,2);
            m.genre       = cellStr(ws,r,3); m.showtime = cellStr(ws,r,4);
            m.totalSeats  = std::stoi(cellStr(ws,r,5).empty()?"0":cellStr(ws,r,5));
            m.bookedSeats = std::stoi(cellStr(ws,r,6).empty()?"0":cellStr(ws,r,6));
            m.price       = std::stod(cellStr(ws,r,7).empty()?"0":cellStr(ws,r,7));
            m.description = cellStr(ws,r,8); m.hallId = cellStr(ws,r,9);
            if (!m.id.empty()) movies.push_back(m);
        }
    } catch (const std::exception& e) { std::cerr << "[DataManager] movies: " << e.what() << "\n"; }
    return movies;
}

// ════════════════════════════════════════════════════════
//  RATINGS
// ════════════════════════════════════════════════════════
void DataManager::saveRatings(const std::vector<Movie>& movies) {
    std::filesystem::create_directories(getExeDir() + "/data");
    xlnt::workbook wb; auto ws = wb.active_sheet(); ws.title("Ratings");
    ws.cell(1,1).value("MovieID"); ws.cell(2,1).value("Username");
    ws.cell(3,1).value("Stars");   ws.cell(4,1).value("Comment");
    ws.cell(5,1).value("Date");
    int r = 2;
    for (auto& m : movies)
        for (auto& rt : m.ratings) {
            ws.cell(1,r).value(m.id);       ws.cell(2,r).value(rt.username);
            ws.cell(3,r).value(rt.stars);   ws.cell(4,r).value(rt.comment);
            ws.cell(5,r).value(rt.date);    r++;
        }
    saveWorkbook(wb, RATINGS_FILE);
}

void DataManager::loadRatings(std::vector<Movie>& movies) {
    if (!std::filesystem::exists(RATINGS_FILE)) return;
    try {
        xlnt::workbook wb; wb.load(RATINGS_FILE);
        auto ws = wb.active_sheet();
        int rows = ws.highest_row();
        for (int r = 2; r <= rows; r++) {
            std::string mid = cellStr(ws,r,1);
            Rating rt;
            rt.username = cellStr(ws,r,2);
            rt.stars    = std::stoi(cellStr(ws,r,3).empty()?"0":cellStr(ws,r,3));
            rt.comment  = cellStr(ws,r,4); rt.date = cellStr(ws,r,5);
            for (auto& m : movies) if (m.id==mid) { m.ratings.push_back(rt); break; }
        }
    } catch (...) {}
}

// ════════════════════════════════════════════════════════
//  TICKETS  — seats stored as comma-separated string
// ════════════════════════════════════════════════════════
void DataManager::saveTickets(const std::vector<Ticket>& tickets) {
    std::filesystem::create_directories(getExeDir() + "/data");
    xlnt::workbook wb; auto ws = wb.active_sheet(); ws.title("Tickets");
    ws.cell(1,1).value("TicketID"); ws.cell(2,1).value("MovieID");
    ws.cell(3,1).value("MovieTitle"); ws.cell(4,1).value("Customer");
    ws.cell(5,1).value("Showtime"); ws.cell(6,1).value("HallID");
    ws.cell(7,1).value("Seats");    ws.cell(8,1).value("SeatCount");
    ws.cell(9,1).value("TotalPrice"); ws.cell(10,1).value("BookingDate");
    ws.cell(11,1).value("Status");
    for (int i = 0; i < (int)tickets.size(); i++) {
        int r = i+2; const Ticket& t = tickets[i];
        ws.cell(1,r).value(t.ticketId);    ws.cell(2,r).value(t.movieId);
        ws.cell(3,r).value(t.movieTitle);  ws.cell(4,r).value(t.customerUsername);
        ws.cell(5,r).value(t.showtime);    ws.cell(6,r).value(t.hallId);
        ws.cell(7,r).value(t.seatsStr());  ws.cell(8,r).value(t.seatCount);
        ws.cell(9,r).value(t.totalPrice);  ws.cell(10,r).value(t.bookingDate);
        ws.cell(11,r).value(t.status);
    }
    saveWorkbook(wb, TICKETS_FILE);
}

std::vector<Ticket> DataManager::loadTickets() {
    std::vector<Ticket> tickets;
    if (!std::filesystem::exists(TICKETS_FILE)) return tickets;
    try {
        xlnt::workbook wb; wb.load(TICKETS_FILE);
        auto ws = wb.active_sheet();
        int rows = ws.highest_row();
        for (int r = 2; r <= rows; r++) {
            Ticket t;
            t.ticketId         = cellStr(ws,r,1);  t.movieId    = cellStr(ws,r,2);
            t.movieTitle       = cellStr(ws,r,3);  t.customerUsername = cellStr(ws,r,4);
            t.showtime         = cellStr(ws,r,5);  t.hallId     = cellStr(ws,r,6);
            // Parse seats from comma-separated string
            std::string seatsStr = cellStr(ws,r,7);
            std::stringstream ss(seatsStr);
            std::string seat;
            while (std::getline(ss, seat, ','))
                if (!seat.empty()) t.seats.push_back(seat);
            t.seatCount  = std::stoi(cellStr(ws,r,8).empty()?"0":cellStr(ws,r,8));
            t.totalPrice = std::stod(cellStr(ws,r,9).empty()?"0":cellStr(ws,r,9));
            t.bookingDate = cellStr(ws,r,10); t.status = cellStr(ws,r,11);
            if (!t.ticketId.empty()) tickets.push_back(t);
        }
    } catch (const std::exception& e) { std::cerr << "[DataManager] tickets: " << e.what() << "\n"; }
    return tickets;
}

// ════════════════════════════════════════════════════════
//  USERS
// ════════════════════════════════════════════════════════
void DataManager::saveUsers(const std::vector<User*>& users) {
    std::filesystem::create_directories(getExeDir() + "/data");
    xlnt::workbook wb; auto ws = wb.active_sheet(); ws.title("Users");
    ws.cell(1,1).value("Username"); ws.cell(2,1).value("Password");
    ws.cell(3,1).value("Role");     ws.cell(4,1).value("DisplayName");
    for (int i = 0; i < (int)users.size(); i++) {
        int r = i+2;
        ws.cell(1,r).value(users[i]->username);    ws.cell(2,r).value(users[i]->password);
        ws.cell(3,r).value(users[i]->roleToString()); ws.cell(4,r).value(users[i]->displayName);
    }
    saveWorkbook(wb, USERS_FILE);
}

std::vector<User*> DataManager::loadUsers() {
    std::vector<User*> users;
    if (!std::filesystem::exists(USERS_FILE)) return users;
    try {
        xlnt::workbook wb; wb.load(USERS_FILE);
        auto ws = wb.active_sheet();
        int rows = ws.highest_row();
        for (int r = 2; r <= rows; r++) {
            std::string uname=cellStr(ws,r,1), pass=cellStr(ws,r,2);
            std::string role=cellStr(ws,r,3),  name=cellStr(ws,r,4);
            if (uname.empty()) continue;
            User* u = nullptr;
            if      (role=="Admin") u = new Admin(uname,pass,name);
            else if (role=="Staff") u = new Staff(uname,pass,name);
            else                    u = new Customer(uname,pass,name);
            users.push_back(u);
        }
    } catch (const std::exception& e) { std::cerr << "[DataManager] users: " << e.what() << "\n"; }
    return users;
}
