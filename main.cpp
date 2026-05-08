#include <iostream>
#include "AuthManager.h"
#include "MenuManager.h"

int main() {
    try {
        AuthManager auth;
        MenuManager menu(auth);
        menu.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
