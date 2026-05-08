#pragma once
#include <vector>
#include <string>
#include "User.h"

class AuthManager {
public:
    AuthManager();
    ~AuthManager();

    // Returns pointer to logged-in user, nullptr if failed
    User* login(const std::string& username, const std::string& password);
    void  logout();

    void  addUser(const std::string& username, const std::string& password,
                  Role role, const std::string& displayName);
    void  removeUser(const std::string& username);
    void  listUsers() const;

    const std::vector<User*>& getUsers() const { return users; }
    void saveUsers() const;

private:
    std::vector<User*> users;
    void loadUsers();
    void seedDefaultUsers();
};
