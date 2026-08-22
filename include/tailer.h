#pragma once

#include <string>
#include "log_entry.h"
#include "thread_safe_queue.h"

/*  Watches one log files fornew lines being written into it, and pushes each line
    into shared queue as a LogEntry.
    One tailer runs per log file, each on its own thread.   */

    void watchLogFile (const std:: string& logFilePath, ThreadSafeQueue<LogEntry>& sharedQueue);