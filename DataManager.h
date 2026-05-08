#pragma once
#include <vector>
#include <string>
#include <xlnt/xlnt.hpp>
#include "Movie.h"
#include "Ticket.h"
#include "User.h"
#include "Hall.h"

class DataManager {
public:
    static const std::string MOVIES_FILE;
    static const std::string TICKETS_FILE;
    static const std::string USERS_FILE;
    static const std::string RATINGS_FILE;
    static const std::string HALLS_FILE;

    static void                saveMovies(const std::vector<Movie>& movies);
    static std::vector<Movie>  loadMovies();

    static void                saveRatings(const std::vector<Movie>& movies);
    static void                loadRatings(std::vector<Movie>& movies);

    static void                saveTickets(const std::vector<Ticket>& tickets);
    static std::vector<Ticket> loadTickets();

    static void                saveUsers(const std::vector<User*>& users);
    static std::vector<User*>  loadUsers();

    static void                saveHalls(const std::vector<Hall>& halls);
    static std::vector<Hall>   loadHalls();

    static std::string generateId(const std::string& prefix, int count);
    static std::string todayDate();

private:
    static std::string cellStr(xlnt::worksheet& ws, int row, int col);
};
