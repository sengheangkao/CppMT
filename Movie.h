#pragma once
#include <string>
#include <vector>

struct Rating {
    std::string username;
    int         stars;
    std::string comment;
    std::string date;
};

class Movie {
public:
    std::string id;
    std::string title;
    std::string genre;
    std::string description;
    std::string showtime;
    std::string hallId;     // which hall this movie plays in
    int         totalSeats;
    int         bookedSeats;
    double      price;
    std::vector<Rating> ratings;

    Movie() : totalSeats(0), bookedSeats(0), price(0.0) {}
    Movie(std::string id, std::string title, std::string genre,
          std::string description, std::string showtime,
          std::string hallId, int seats, double price)
        : id(id), title(title), genre(genre), description(description),
          showtime(showtime), hallId(hallId),
          totalSeats(seats), bookedSeats(0), price(price) {}

    int    availableSeats() const { return totalSeats - bookedSeats; }
    bool   isSoldOut()      const { return availableSeats() <= 0; }

    double averageRating() const {
        if (ratings.empty()) return 0.0;
        double sum = 0;
        for (auto& r : ratings) sum += r.stars;
        return sum / ratings.size();
    }

    static std::string ratingIcons(int stars, bool includeEmpty = true) {
        std::string s;
        if (stars < 0) stars = 0;
        if (stars > 5) stars = 5;
        for (int i = 0; i < stars; i++) s += "⭐️";
        if (includeEmpty) {
            for (int i = stars; i < 5; i++) s += "☆";
        }
        return s;
    }

    std::string starsStr() const {
        double avg = averageRating();
        if (avg == 0) return "No ratings";
        int full = (int)avg;
        std::string s = ratingIcons(full);
        s += " (" + std::to_string(ratings.size()) + ")";
        return s;
    }

    std::string statusStr() const {
        if (isSoldOut())           return "SOLD OUT";
        if (availableSeats() <= 5) return "Almost Full";
        return "Available";
    }
};
