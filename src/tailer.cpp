#include "tailer.h"
#include <sys/inotify.h>        //  Lets us ask the kernel to watch a file for changes
#include <unistd.h>             //  Gives us read() and close()
#include <fstream>              //  Lets us read new lines out of log files
#include <iostream>             //  Used only to print setup errors
#include <chrono>               //  Gives us system_clock, used for entry timestamps

/*  inotify delivers events in a buffer, so we size it big enough to hold several events at once,
    each padded with room for a filename    */

constexpr int EVENT_STRUCT_SIZE = sizeof(struct inotify_event);
constexpr int EVENT_BUFFER_SIZE = (EVENT_STRUCT_SIZE + 16) * 10;

/*  Opens the log file and attaches a fresh inotify watch to it, starting from
    the end so old content already in the file isn't reprocessed.    */

int attachWatch(const std::string& logFilePath, int inotifyInstance, std::ifstream& logFileStream) {
    logFileStream.close();
    logFileStream.clear();
    logFileStream.open(logFilePath);
    logFileStream.seekg(0, std::ios::end);

    // We track modifications, plus the two events that tell us the file itself was renamed or deleted during log rotation.
    
    int watchDescriptor = inotify_add_watch(inotifyInstance, logFilePath.c_str(), IN_MODIFY | IN_MOVE_SELF | IN_DELETE_SELF);
    if (watchDescriptor < 0) {
        std::cerr << "Could not add watch for: " << logFilePath << std::endl;
    }
    return watchDescriptor;
}

void watchLogFile(const std::string& logFilePath, ThreadSafeQueue<LogEntry>& sharedQueue) {

    /*  Step 1 : Create an inotify instance. This gives us a handle (a file descriptor) that represents
        "Our connection to the kernel's file-watching system"   */

    int inotifyInstance = inotify_init();
    if (inotifyInstance < 0) {
        std::cerr << "Could not start watching: " << logFilePath << std::endl;
        return;
    }

    /*  Step 2 : Open the log file, jump to the end, and register our watch on it.   */

    std::ifstream logFileStream;
    int watchDescriptor = attachWatch(logFilePath, inotifyInstance, logFileStream);
    if (watchDescriptor < 0) {
        close(inotifyInstance);
        return;
    }

    char eventBuffer[EVENT_BUFFER_SIZE];

    /*  Sit and wait for changes, forever (until the program exits).    */

    while (true) {

        /*  This call blocks the thread (puts it to sleep) until the kernel actually has an event for us.
            No busy-waiting     */

        int bytesRead = read(inotifyInstance, eventBuffer, EVENT_BUFFER_SIZE);
        if (bytesRead <= 0) {
            continue;
        }

        int offset = 0;
        bool logFileWasRotated = false;

        /*  Step 3 : A single read() can hand back several events packed together,
            so we walk through the buffer one event at a time.    */

        while (offset < bytesRead) {
            struct inotify_event* event = reinterpret_cast<struct inotify_event*>(eventBuffer + offset);

            if (event->mask & (IN_MOVE_SELF | IN_DELETE_SELF)) {
                logFileWasRotated = true;
            }

            offset += EVENT_STRUCT_SIZE + event->len;
        }

        if (logFileWasRotated) {
            /*  Step 4 : The file we were watching got renamed or deleted during rotation.
                Drop the stale watch and reattach to the freshly created file at the same path.   */

            inotify_rm_watch(inotifyInstance, watchDescriptor);
            watchDescriptor = attachWatch(logFilePath, inotifyInstance, logFileStream);
            if (watchDescriptor < 0) {
                break;
            }
            continue;
        }

        /*  Step 5 : Something was written to the file. Read whatever new lines
            got appended since our last read position.   */

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

    /*  Cleanup (unreachable in this simple version, but good practice to
        know it exists for later when we add proper shutdown handling ).  */

    inotify_rm_watch(inotifyInstance, watchDescriptor);
    close(inotifyInstance);
}

