#pragma once
#include <string>
#include <vector>

// One seat in the hall
struct Seat {
    std::string seatId;   // e.g. "A1", "B3"
    bool        booked;
    std::string bookedBy; // username who booked it

    Seat() : booked(false) {}
    Seat(const std::string& id) : seatId(id), booked(false) {}
};

// A hall is a 2D grid of seats: rows (A,B,C...) x cols (1,2,3...)
class Hall {
public:
    std::string              id;       // "HALL_A", "HALL_B"
    std::string              name;     // "Hall A", "Hall B"
    int                      rows;     // number of row letters
    int                      cols;     // seats per row
    std::vector<std::vector<Seat>> seats; // seats[row][col]

    Hall() : rows(0), cols(0) {}
    Hall(const std::string& id, const std::string& name, int rows, int cols)
        : id(id), name(name), rows(rows), cols(cols) {
        initSeats();
    }

    void initSeats() {
        seats.clear();
        seats.resize(rows, std::vector<Seat>(cols));
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++) {
                std::string sid;
                sid += (char)('A' + r);
                sid += std::to_string(c + 1);
                seats[r][c] = Seat(sid);
            }
    }

    int totalSeats()    const { return rows * cols; }
    int bookedSeats()   const {
        int count = 0;
        for (auto& row : seats)
            for (auto& s : row)
                if (s.booked) count++;
        return count;
    }
    int availableSeats() const { return totalSeats() - bookedSeats(); }

    // Find seat by ID like "B3"
    Seat* findSeat(const std::string& seatId) {
        for (auto& row : seats)
            for (auto& s : row)
                if (s.seatId == seatId) return &s;
        return nullptr;
    }

    bool bookSeat(const std::string& seatId, const std::string& username) {
        Seat* s = findSeat(seatId);
        if (!s || s->booked) return false;
        s->booked   = true;
        s->bookedBy = username;
        return true;
    }

    bool cancelSeat(const std::string& seatId) {
        Seat* s = findSeat(seatId);
        if (!s || !s->booked) return false;
        s->booked   = false;
        s->bookedBy = "";
        return true;
    }
};
