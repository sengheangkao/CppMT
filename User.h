#pragma once
#include <string>

enum class Role { ADMIN, STAFF, CUSTOMER };

// ─────────────────────────────────────────────
//  Base User class
// ─────────────────────────────────────────────
class User {
public:
    std::string username;
    std::string password;
    Role        role;
    std::string displayName;

    User() = default;
    User(std::string u, std::string p, Role r, std::string name)
        : username(u), password(p), role(r), displayName(name) {}

    virtual ~User() = default;

    std::string roleToString() const {
        switch (role) {
            case Role::ADMIN:    return "Admin";
            case Role::STAFF:    return "Staff";
            case Role::CUSTOMER: return "Customer";
        }
        return "Unknown";
    }

    // Each subclass shows its own menu
    virtual void showMenu() = 0;
};

// ─────────────────────────────────────────────
//  Admin  – full access
// ─────────────────────────────────────────────
class Admin : public User {
public:
    Admin(std::string u, std::string p, std::string name)
        : User(u, p, Role::ADMIN, name) {}
    void showMenu() override;
};

// ─────────────────────────────────────────────
//  Staff  – manage movies & tickets
// ─────────────────────────────────────────────
class Staff : public User {
public:
    Staff(std::string u, std::string p, std::string name)
        : User(u, p, Role::STAFF, name) {}
    void showMenu() override;
};

// ─────────────────────────────────────────────
//  Customer  – buy & view own tickets
// ─────────────────────────────────────────────
class Customer : public User {
public:
    Customer(std::string u, std::string p, std::string name)
        : User(u, p, Role::CUSTOMER, name) {}
    void showMenu() override;
};
