#pragma once
#include <vector>
#include "User.h"
#include "Movie.h"
#include "Ticket.h"
#include "Hall.h"
#include "AuthManager.h"

class MenuManager {
public:
    MenuManager(AuthManager& auth);
    void run();

private:
    AuthManager&        auth;
    std::vector<Movie>  movies;
    std::vector<Ticket> tickets;
    std::vector<Hall>   halls;
    User*               currentUser = nullptr;

    // Auth
    void showNowPlaying();
    void showStartingSoon();
    bool doLogin();
    void doRegister();
    void runUserSession();
    void dispatchAdmin(int choice);
    void dispatchStaff(int choice);
    void dispatchCustomer(int choice);

    // Movies
    void listMovies(bool showAll = true);
    void viewMovieDetail();
    void addMovie();
    void editMovie();
    void deleteMovie();
    void searchMovies();

    // Halls
    void listHalls();
    void addHall();
    void editHall();
    void deleteHall();
    void viewHallSeats(const std::string& hallId, const std::string& movieId);
    void viewByHall();

    // Ratings
    void rateMovie();

    // Tickets
    void bookTicket();
    void cancelTicket();
    void viewMyTickets();
    void viewAllTickets();
    void viewHistory();
    void viewTicketDetails();
    void showTimeRecommendations();

    // Admin
    void manageUsers();
    void viewReports();

    // Helpers
    void        printBanner();
    void        printHeader(const std::string& title);
    void        printSeparator();
    void        showLoading(const std::string& title, const std::string& subtitle);
    void        showInlineLoading(const std::string& title, const std::string& subtitle);
    void        printTicketReceipt(const Ticket& ticket, bool showTitle);
    std::string saveTicketReceipt(const Ticket& ticket);
    int         getIntInput(const std::string& prompt, int min, int max);
    int         getValidInt(const std::string& prompt);
    double      getDoubleInput(const std::string& prompt);
    std::string getStringInput(const std::string& prompt);
    void        waitForEnter();
    void        clearScreen();
    Movie*      findMovie(const std::string& id);
    Ticket*     findTicket(const std::string& id);
    Hall*       findHall(const std::string& id);
    void        saveAll();
};
