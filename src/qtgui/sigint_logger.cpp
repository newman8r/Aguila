#include "sigint_logger.h"
#include <iostream>

SigintLogger::SigintLogger() : logStream(nullptr) {
    // Force stderr to be unbuffered
    setvbuf(stderr, nullptr, _IONBF, 0);
}

SigintLogger::~SigintLogger() {
    cleanup();
}

SigintLogger& SigintLogger::instance() {
    static SigintLogger logger;
    return logger;
}

void SigintLogger::initialize(const QString& logPath) {
    instance().initializeImpl(logPath);
}

void SigintLogger::debug(const QString& msg) {
    instance().logImpl("Debug", "\033[34m", msg);  // Blue
}

void SigintLogger::info(const QString& msg) {
    instance().logImpl("Info", "\033[32m", msg);   // Green
}

void SigintLogger::warning(const QString& msg) {
    instance().logImpl("Warning", "\033[33m", msg); // Yellow
}

void SigintLogger::error(const QString& msg) {
    instance().logImpl("Error", "\033[31m", msg);   // Red
}

void SigintLogger::cleanup() {
    instance().cleanupImpl();
}

void SigintLogger::initializeImpl(const QString& logPath) {
    if (logFile.isOpen()) return;
    
    logFile.setFileName(logPath);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        logStream = new QTextStream(&logFile);
        
        // Write startup message to both console and file
        QString startMsg = QString("\n=== Starting GQRX SIGINT at %1 ===\n")
                            .arg(QDateTime::currentDateTime().toString());
        
        fprintf(stderr, "\033[32m%s\033[0m", qPrintable(startMsg));  // Green
        *logStream << startMsg;
        logStream->flush();
    } else {
        fprintf(stderr, "\033[31mFailed to open log file: %s\033[0m\n", 
                qPrintable(logPath));
    }
}

void SigintLogger::logImpl(const QString& level, const char* color, const QString& msg) {
    QString timestamp = QDateTime::currentDateTime()
                        .toString("yyyy-MM-dd hh:mm:ss.zzz");
    
    // Console output with color - using fprintf for direct output
    fprintf(stderr, "%s[%s][%s]: %s\033[0m\n",
            color,
            qPrintable(timestamp),
            qPrintable(level),
            qPrintable(msg));
    
    // File output
    if (logStream) {
        *logStream << QString("[%1][%2]: %3\n")
                        .arg(timestamp)
                        .arg(level)
                        .arg(msg);
        logStream->flush();
    }
}

void SigintLogger::cleanupImpl() {
    if (logStream) {
        delete logStream;
        logStream = nullptr;
    }
    if (logFile.isOpen()) {
        logFile.close();
    }
} 