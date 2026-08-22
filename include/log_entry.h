#pragma once

#include <string>
#include <chrono>

/*  Represents one line captured from a log file, along with
    context about where it came from and when it was seen    */

    struct LogEntry {
        std:: string sourceFilePath;        //  Path of the log file this line belongs to
        std:: string lineContent;           //  The actual text of the log line
        std:: chrono::system_clock  :: time_point capturedAt;           //   Moment this line was read
    };
    