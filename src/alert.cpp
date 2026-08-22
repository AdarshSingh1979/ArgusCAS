#include "alert.h"
#include <iostream>

void sendConsoleAlert(const std::string& alertMessage) {
    std::cout << "[ALERT] " << alertMessage << std::endl;
}