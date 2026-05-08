#include "AuthManager.h"
#include "DataManager.h"
#include "tabulate.hpp"
#include <iostream>
#include <algorithm>

AuthManager::AuthManager() {
    loadUsers();
    if (users.empty()) seedDefaultUsers();
}

AuthManager::~AuthManager() {
    for (auto* u : users) delete u;
}

void AuthManager::loadUsers() {
    users = DataManager::loadUsers();
}

// Default accounts so the app works on first run
void AuthManager::seedDefaultUsers() {
    users.push_back(new Admin("admin",    "admin123",  "Admin User"));
    users.push_back(new Staff("staff1",   "staff123",  "Staff Member"));
    users.push_back(new Customer("alice", "alice123",  "Alice Smith"));
    users.push_back(new Customer("bob",   "bob123",    "Bob Johnson"));
    DataManager::saveUsers(users);
}

User* AuthManager::login(const std::string& username, const std::string& password) {
    for (auto* u : users) {
        if (u->username == username && u->password == password)
            return u;
    }
    return nullptr;
}

void AuthManager::logout() {
    // nothing stateful to clear — currentUser is owned by MenuManager
}

void AuthManager::addUser(const std::string& username, const std::string& password,
                          Role role, const std::string& displayName) {
    // Check duplicate
    for (auto* u : users) {
        if (u->username == username) {
            std::cout << "  Username already exists!\n";
            return;
        }
    }
    User* u = nullptr;
    if      (role == Role::ADMIN)    u = new Admin(username, password, displayName);
    else if (role == Role::STAFF)    u = new Staff(username, password, displayName);
    else                             u = new Customer(username, password, displayName);
    users.push_back(u);
    DataManager::saveUsers(users);
    std::cout << "  User '" << username << "' created successfully!\n";
}

void AuthManager::removeUser(const std::string& username) {
    auto it = std::find_if(users.begin(), users.end(),
        [&username](User* u){ return u->username == username; });
    if (it == users.end()) {
        std::cout << "  User not found!\n";
        return;
    }
    delete *it;
    users.erase(it);
    DataManager::saveUsers(users);
    std::cout << "  User removed.\n";
}

void AuthManager::listUsers() const {
    tabulate::Table t;
    t.addHeader({"Username", "Role", "Display Name"});
    for (auto* u : users)
        t.addRow({u->username, u->roleToString(), u->displayName});
    std::cout << "\n";
    t.print();
}

void AuthManager::saveUsers() const {
    DataManager::saveUsers(users);
}
