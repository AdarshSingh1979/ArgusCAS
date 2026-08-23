#pragma once

#include "thread_safe_queue.h"
#include "log_entry.h"
#include "alert.h"
#include <string>
#include <deque>
#include <chrono>

/*  Watches fora burst of matching log lines within a short time window,
    and fires an alert if too many show up too quickly.
    one instance of this class tracks one rule ( e.g. too many ERRORS )    */

    class RuleEngine {
        public:
        /*  wordToMatch : the text a log line must contain to count (e.g. "ERROR" )
            maxAllowedInWindow : how many matches are allowed before alerting
            windowDurationSeconds : the size of sliding time window     */
            
            RuleEngine(std::string wordToMatch, int maxAllowedInWindow, int windowDurationSeconds);

        /*  Keeps pulling log lines from shared queue forever, and checks each one against this rule.   
            Meant to run on its own thread */

            void startWatching( ThreadSafeQueue<LogEntry>& sharedQueue );

        private:
            std:: string wordToMatch;
            int maxAllowedInWindow;
            int windowDurationSeconds;

            // Timestamps of recent matching lines, oldest first.
            // This is our sliding window

            std:: deque<std:: chrono:: system_clock:: time_point> recentMatchTimestamps;

            // Adds a new match, drops timestamps older than the window,
            // and returns true if count now exceeds our limit.

            bool shouldFireAlert( std:: chrono:: system_clock:: time_point newMatchTime);
    };