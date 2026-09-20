#include "logger.h"
#include <QDir>
#include <QThread>
#include <QCoreApplication>
#include <QStandardPaths>

Logger* Logger::m_instance = nullptr;

Logger::Logger(QObject* parent) : QObject(parent),
    m_minLevel(Info), m_outputToConsole(true), m_maxFileSize(10), m_maxBackup(5)
{
}

Logger::~Logger()
{
    if (m_logFile.isOpen())
        m_logFile.close();
}

Logger* Logger::instance()
{
    if (!m_instance) {
        m_instance = new Logger();
    }
    return m_instance;
}

void Logger::initialize(const QString& logDir, LogLevel minLevel, bool outputToConsole, int maxFileSizeMB, int maxBackupFiles)
{
    m_minLevel = minLevel;
    m_outputToConsole = outputToConsole;
    m_maxFileSize = maxFileSizeMB * 1024 * 1024;
    m_maxBackup = maxBackupFiles;

    QDir dir(logDir);
    if (!dir.exists())
        dir.mkpath(".");

    QString date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    m_currentLogFileName = logDir + "/app_" + date + ".log";
    m_logFile.setFileName(m_currentLogFileName);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_stream.setDevice(&m_logFile);
    }

    qInstallMessageHandler(messageHandler);
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    Logger::instance()->writeLog(type, msg,
                                 context.file ? QFileInfo(context.file).fileName() : "unknown",
                                 context.line);
}

void Logger::writeLog(QtMsgType type, const QString& msg, const QString& file, int line)
{
    QMutexLocker locker(&m_mutex);

    LogLevel msgLevel;
    switch (type) {
    case QtDebugMsg:     msgLevel = Debug; break;
    case QtInfoMsg:      msgLevel = Info; break;
    case QtWarningMsg:   msgLevel = Warning; break;
    case QtCriticalMsg:  msgLevel = Critical; break;
    case QtFatalMsg:     msgLevel = Critical; break;
    default:             msgLevel = Debug; break;
    }

    if (msgLevel < m_minLevel)
        return;

    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString threadStr = QString::number((quintptr)QThread::currentThreadId(), 16);
    QString levelStr;
    switch (type) {
    case QtDebugMsg:    levelStr = "DEBUG"; break;
    case QtInfoMsg:     levelStr = "INFO "; break;
    case QtWarningMsg:  levelStr = "WARN "; break;
    case QtCriticalMsg: levelStr = "ERROR"; break;
    case QtFatalMsg:    levelStr = "FATAL"; break;
    default:            levelStr = "UNKWN"; break;
    }

    QString logLine = QString("[%1] [%2] [%3] %4:%5 - %6")
                          .arg(timeStr, threadStr, levelStr, file, QString::number(line), msg);

    if (m_outputToConsole) {
        fprintf(stderr, "%s\n", qPrintable(logLine));
        fflush(stderr);
    }

    if (m_logFile.isOpen()) {
        m_stream << logLine << "\n";
        m_stream.flush();
        rotateIfNeeded();
    }
}

void Logger::rotateIfNeeded()
{
    if (m_logFile.size() < m_maxFileSize)
        return;
    m_logFile.close();

    for (int i = m_maxBackup; i >= 1; --i) {
        QString oldName = m_currentLogFileName + QString(".%1").arg(i);
        QString newName = m_currentLogFileName + QString(".%1").arg(i+1);
        if (i == m_maxBackup && QFile::exists(oldName))
            QFile::remove(oldName);
        if (QFile::exists(oldName))
            QFile::rename(oldName, newName);
    }
    QString backupName = m_currentLogFileName + ".1";
    QFile::rename(m_currentLogFileName, backupName);

    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        m_stream.setDevice(&m_logFile);
}
