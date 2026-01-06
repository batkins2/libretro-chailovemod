#include "LibretroLog.h"
#include <iostream>

retro_log_printf_t log_cb = nullptr;

std::ostream& LibretroLog::log(enum retro_log_level level) {
    static LoggerBuf infoBuf(RETRO_LOG_INFO);
    static LoggerBuf errorBuf(RETRO_LOG_ERROR);
    static std::ostream infoStream(&infoBuf);
    static std::ostream errorStream(&errorBuf);
    switch (level) {
        case RETRO_LOG_ERROR:
            return errorStream;
        case RETRO_LOG_INFO:
        default:
            return infoStream;
    }
}

void LibretroLog::flushLog() {
    // No-op for now
}

int LibretroLog::LoggerBuf::sync() {
    if (log_cb) {
        std::string msg = str();
        log_cb(level, "%s", msg.c_str());
    }
    str("");
    return 0;
}
