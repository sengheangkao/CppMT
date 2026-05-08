#pragma once
#include <string>
#include <vector>

class Ticket {
public:
    std::string ticketId;
    std::string movieId;
    std::string movieTitle;
    std::string customerUsername;
    std::string showtime;
    std::string hallId;
    std::vector<std::string> seats;   // e.g. ["A1","A2"]
    int         seatCount;
    double      totalPrice;
    std::string bookingDate;
    std::string status;

    Ticket() : seatCount(0), totalPrice(0.0), status("Confirmed") {}
    Ticket(std::string tid, std::string mid, std::string mtitle,
           std::string cust, std::string show, std::string hall,
           std::vector<std::string> seats, double price, std::string date)
        : ticketId(tid), movieId(mid), movieTitle(mtitle),
          customerUsername(cust), showtime(show), hallId(hall),
          seats(seats), seatCount((int)seats.size()),
          totalPrice(price), bookingDate(date), status("Confirmed") {}

    std::string seatsStr() const {
        std::string s;
        for (int i = 0; i < (int)seats.size(); i++) {
            s += seats[i];
            if (i < (int)seats.size()-1) s += ",";
        }
        return s;
    }
};
