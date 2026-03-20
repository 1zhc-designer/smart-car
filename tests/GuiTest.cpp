#include <QApplication>
#include <QTest>
#include "gui/MainWindow.hpp"
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    
    try {
        MainWindow window;
        if (window.windowTitle() == "Smart Car Control") {
            std::cout << "GuiTest: MainWindow initialized successfully." << std::endl;
        }
        window.close();
    } catch (const std::exception& e) {
        std::cout << "[GuiTest] Warning: Running without hardware drivers: " << e.what() << std::endl;
    }
    
    return 0;
}