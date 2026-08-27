#pragma once

#include "thread_safe_queue.h"
#include "log_entry.h"
#include "alert.h"
#include <string>
#include <deque>
#include <chrono>

/*  Fires an alert if a target word shows up too many times within a sliding time window.
    One instance tracks one rule (e.g. too many ERRORs).   */

    class RuleEngine {
        public:
        /*  wordToMatch : the text a log line must contain to count (e.g. "ERROR" )
            maxAllowedInWindow : how many matches are allowed before alerting
            windowDurationSeconds : the size of sliding time window     */
            
            RuleEngine(std::string wordToMatch, int maxAllowedInWindow, int windowDurationSeconds);

        // pulls from sharedQueue forever, checking each line against this rule. runs on its own thread

            void startWatching( ThreadSafeQueue<LogEntry>& sharedQueue );

        private:
            std:: string wordToMatch;
            int maxAllowedInWindow;
            int windowDurationSeconds;
            // sliding window: timestamps of recent matches, oldest first

            std:: deque<std:: chrono:: system_clock:: time_point> recentMatchTimestamps;

            bool shouldFireAlert( std:: chrono:: system_clock:: time_point newMatchTime);
    };