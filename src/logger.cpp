#include "logger.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>

static FILE* g_LogFile = nullptr;
static bool g_LoggingEnabled = false;
static CRITICAL_SECTION g_LogLock;
static bool g_LockInit = false;

namespace Logger {
    void Init(const std::string& logFileName, bool enableLogging) {
        if (!g_LockInit) {
            InitializeCriticalSection(&g_LogLock);
            g_LockInit = true;
        }

        g_LoggingEnabled = enableLogging;
        if (!g_LoggingEnabled) return;

        g_LogFile = fopen(logFileName.c_str(), "w");
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
        if (!g_LoggingEnabled || !g_LogFile || !g_LockInit) return;

        EnterCriticalSection(&g_LogLock);
        if (!g_LogFile) { // re-check after acquiring the lock (Close may have run)
            LeaveCriticalSection(&g_LogLock);
            return;
        }

        time_t rawtime;
        struct tm* timeinfo;
        char timeBuf[32];

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", timeinfo);

        fprintf(g_LogFile, "[%s] ", timeBuf);

        va_list args;
        va_start(args, format);
        vfprintf(g_LogFile, format, args);
        va_end(args);

        fprintf(g_LogFile, "\n");
        fflush(g_LogFile);
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
