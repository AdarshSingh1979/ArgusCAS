#include "rule_engine.h"

RuleEngine::RuleEngine(std::string wordToMatch, int maxAllowedInWindow, int windowDurationSeconds)
    : wordToMatch(wordToMatch),
      maxAllowedInWindow(maxAllowedInWindow),
      windowDurationSeconds(windowDurationSeconds)  {}

    bool RuleEngine:: shouldFireAlert(std:: chrono :: system_clock:: time_point newMatchTime){
        //  Record this new match before checking the window.
        recentMatchTimestamps.push_back(newMatchTime);
        
        //  Drop every timestamp that has fallen outside the window, starting from the oldest entry at the front.

        while(!recentMatchTimestamps.empty()){
            auto oldestMatchTime = recentMatchTimestamps.front();
            auto secondSinceOldestMatch = std:: chrono :: duration_cast<std::chrono::seconds>(newMatchTime - oldestMatchTime).count();

            if( secondSinceOldestMatch > windowDurationSeconds ){
                recentMatchTimestamps.pop_front();
            }else {
                break;
            }
        }
        return static_cast<int>(recentMatchTimestamps.size()) >= maxAllowedInWindow;
    }

    void RuleEngine:: startWatching( ThreadSafeQueue<LogEntry>& sharedQueue){
        while(true){
            LogEntry entry = sharedQueue.popItem();

            //  Only lines containing our target word count towards this rule.
            if( entry.lineContent.find(wordToMatch) != std:: string:: npos){
                if( shouldFireAlert(entry.capturedAt) ){
                    std:: string message = "Detected" + std:: to_string( maxAllowedInWindow)+ 
                   "occurrences of \"" + wordToMatch + "\" within "+ std:: to_string(windowDurationSeconds) + " seconds.";

                   sendConsoleAlert(message);
                }
            }
        }
    }
