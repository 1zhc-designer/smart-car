#include "gui/SystemFacade.hpp"

#include <iostream>

/**
 * @brief Command-line entry point for the smart car runtime.
 */
int main() {
    try {
        SystemFacade system;
        system.start();

        std::cout << "DDS-style smart car runtime started.\n";
        std::cout << "GUI commands and IR commands share the same pub/sub control path.\n";
        std::cout << "Press Enter to stop the program...\n";
        std::cin.get();

        system.stop();
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Fatal: " << exception.what() << "\n";
        return 1;
    }
}
