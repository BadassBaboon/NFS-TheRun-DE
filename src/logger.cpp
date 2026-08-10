#include "logger.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>

static FILE* g_LogFile = nullptr;
static bool g_LoggingEnabled = false;
static CRITICAL_SECTION g_LogLock;
static bool g_LockInit = false;

// If the log file cannot be opened, every line goes to the debugger channel
// instead, where DebugView will pick it up. The mod is not the right place to
// work around a folder it cannot write to — but a user with nothing at all to
// send makes a problem impossible to diagnose remotely, and this costs nothing.
static bool g_MirrorToDebugger = false;

namespace {
    // Writes a line to the debugger channel. Free when nothing is listening.
    void Emit(const char* text) {
        OutputDebugStringA(text);
    }
}

namespace Logger {
    void Init(const std::string& logFileName, bool enableLogging) {
        if (!g_LockInit) {
            InitializeCriticalSection(&g_LogLock);
            g_LockInit = true;
        }

        g_LoggingEnabled = enableLogging;
        if (!g_LoggingEnabled) return;

        // Next to the .asi, which is where people look for it.
        g_LogFile = fopen(logFileName.c_str(), "w");

        if (!g_LogFile) {
            // Keep logging rather than going silent, so the failure is still
            // visible to anyone who attaches DebugView.
            g_MirrorToDebugger = true;
            Emit("[NFSTR-DE] could not open the log file; mirroring to the debugger only\n");
        }

        if (g_LogFile) {
            time_t rawtime;
            struct tm* timeinfo;
            char timeBuf[80];

            time(&rawtime);
            timeinfo = localtime(&rawtime);
            strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", timeinfo);

            fprintf(g_LogFile, "====================================================\n");
            fprintf(g_LogFile, "  NFS The Run Definitive Edition (NFSTR-DE) v1.0.0  \n");
            fprintf(g_LogFile, "  Log initialized at: %s\n", timeBuf);
            fprintf(g_LogFile, "====================================================\n\n");
            fflush(g_LogFile);
        }
    }

    void Log(const char* format, ...) {
        if (!g_LoggingEnabled || !g_LockInit) return;
        if (!g_LogFile && !g_MirrorToDebugger) return;

        EnterCriticalSection(&g_LogLock);
        // Re-check after acquiring the lock, in case Close() ran in between.
        if (!g_LogFile && !g_MirrorToDebugger) {
            LeaveCriticalSection(&g_LogLock);
            return;
        }

        time_t rawtime;
        struct tm* timeinfo;
        char timeBuf[32];

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", timeinfo);

        // Formatted once into a buffer so the file and the debugger channel get
        // the same text; a va_list cannot be walked twice.
        char body[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(body, sizeof(body), format, args);
        va_end(args);

        if (g_LogFile) {
            fprintf(g_LogFile, "[%s] %s\n", timeBuf, body);
            fflush(g_LogFile);
        }
        if (g_MirrorToDebugger) {
            char line[1152];
            snprintf(line, sizeof(line), "[NFSTR-DE] [%s] %s\n", timeBuf, body);
            Emit(line);
        }
        LeaveCriticalSection(&g_LogLock);
    }

    void Close() {
        if (!g_LockInit) return;

        EnterCriticalSection(&g_LogLock);
        if (g_LogFile) {
            fclose(g_LogFile);
            g_LogFile = nullptr;
        }
        LeaveCriticalSection(&g_LogLock);
    }
}
