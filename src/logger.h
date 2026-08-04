#ifndef LOGGER_H
#define LOGGER_H

#include <string>

namespace Logger {
    void Init(const std::string& logFileName, bool enableLogging);
    void Log(const char* format, ...);
    void Close();
}

#endif // LOGGER_H
