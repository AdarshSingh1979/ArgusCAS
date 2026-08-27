#include "tailer.h"
#include <sys/inotify.h>        //  ask the kernel to watch a file for changes
#include <unistd.h>             //  Gives read() and close()
#include <fstream>              //  read new lines out of log files
#include <iostream>             //  Used only to print setup errors
#include <chrono>               //  Gives system_clock, used for entry timestamps

// inotify delivers events in a buffer, size it for several events at once with room for filenames.

constexpr int EVENT_STRUCT_SIZE = sizeof(struct inotify_event);
constexpr int EVENT_BUFFER_SIZE = (EVENT_STRUCT_SIZE + 16) * 10;

// opens the log file and attaches a fresh watch, starting from the end so old content isn't reprocessed

int attachWatch(const std::string& logFilePath, int inotifyInstance, std::ifstream& logFileStream) {
    logFileStream.close();
    logFileStream.clear();
    logFileStream.open(logFilePath);
    logFileStream.seekg(0, std::ios::end);

    // track modifications, plus rename/delete so we notice log rotation
    int watchDescriptor = inotify_add_watch(inotifyInstance, logFilePath.c_str(), IN_MODIFY | IN_MOVE_SELF | IN_DELETE_SELF);
    if (watchDescriptor < 0) {
        std::cerr << "Could not add watch for: " << logFilePath << std::endl;
    }
    return watchDescriptor;
}

void watchLogFile(const std::string& logFilePath, ThreadSafeQueue<LogEntry>& sharedQueue) {

    int inotifyInstance = inotify_init();
    if (inotifyInstance < 0) {
        std::cerr << "Could not start watching: " << logFilePath << std::endl;
        return;
    }

    std::ifstream logFileStream;
    int watchDescriptor = attachWatch(logFilePath, inotifyInstance, logFileStream);
    if (watchDescriptor < 0) {
        close(inotifyInstance);
        return;
    }

    char eventBuffer[EVENT_BUFFER_SIZE];

    while (true) {
        int bytesRead = read(inotifyInstance, eventBuffer, EVENT_BUFFER_SIZE);
        if (bytesRead <= 0) {
            continue;
        }

        int offset = 0;
        bool logFileWasRotated = false;

         // a single read() can hand back several events packed together
        while (offset < bytesRead) {
            struct inotify_event* event = reinterpret_cast<struct inotify_event*>(eventBuffer + offset);

            if (event->mask & (IN_MOVE_SELF | IN_DELETE_SELF)) {
                logFileWasRotated = true;
            }

            offset += EVENT_STRUCT_SIZE + event->len;
        }

        if (logFileWasRotated) {
             // file got renamed/deleted during rotation, reattach at the same path */

            inotify_rm_watch(inotifyInstance, watchDescriptor);
            watchDescriptor = attachWatch(logFilePath, inotifyInstance, logFileStream);
            if (watchDescriptor < 0) {
                break;
            }
            continue;
        }

        logFileStream.clear();
        std::string newLine;
        while (std::getline(logFileStream, newLine)) {
            LogEntry entry;
            entry.sourceFilePath = logFilePath;
            entry.lineContent = newLine;
            entry.capturedAt = std::chrono::system_clock::now();

            sharedQueue.pushItem(entry);
        }
    }

    inotify_rm_watch(inotifyInstance, watchDescriptor);
    close(inotifyInstance);
}

