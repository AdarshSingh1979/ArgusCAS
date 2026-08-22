#pragma once

#include <string>

/*  Prints an alert message to the console when a monitoring rule
    is triggered by the rule engine.    */

void sendConsoleAlert(const std::string& alertMessage);