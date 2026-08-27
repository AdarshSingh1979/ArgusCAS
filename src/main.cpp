#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include "log_entry.h"
#include "rule_engine.h"
#include "thread_safe_queue.h"
#include "tailer.h"

// Set to true the moment Ctrl+C is pressed. Marked atomic since the signal handler and the main loop touch it from different threads.
std::atomic<bool> shutdownRequested(false);

void handleShutdownSignal(int signalNumber) {
    shutdownRequested = true;
}

int main(int argc, char*argv[]){
    if( argc < 2 ){
        std:: cerr<< "Usage: " << argv[0] << "<log_file_path>" <<std::endl;
        return 1;
    }
    std:: string logFilePath = argv[1];
    
    //  Catch Ctrl+C so we can shut down with a clean message instead of the OS killing the process mid-operation.
    std::signal(SIGINT, handleShutdownSignal);

    ThreadSafeQueue<LogEntry> sharedQueue;

    std:: thread tailerThread( watchLogFile, logFilePath, std::ref(sharedQueue));
    tailerThread.detach();

    //  fires alert if "ERROR" shows up 3+ times in 10 seconds.
    RuleEngine errorRule("ERROR",3,10);
    std:: thread ruleEngineThread(&RuleEngine::startWatching, &errorRule, std::ref(sharedQueue));
    ruleEngineThread.detach();

    std:: cout<< "Log Monitor is now watching : " << logFilePath <<std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;

    //  Sleep here instead of busy-checking
    while (!shutdownRequested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\nShutdown signal received. Exiting Log Monitor." << std::endl;

    return 0;
}