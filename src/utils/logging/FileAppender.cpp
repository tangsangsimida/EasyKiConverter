#include "FileAppender.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace EasyKiConverter {

// 允许的日志目录列表（相对于用户数据目录或应用目录）
static const QStringList s_allowedDirs = {
    QString(),  // 应用程序目录
    "logs",     // logs 子目录
    "log",      // log 子目录
};

FileAppender::FileAppender(const QString& filePath, qint64 maxSize, int maxFiles, bool async)
    : m_maxSize(maxSize), m_maxFiles(qMax(1, maxFiles)), m_async(async) {
    // 验证和规范化路径
    if (!validateAndNormalizePath(filePath)) {
        m_lastError = "Invalid or unsafe file path: " + filePath;
        m_pathValid = false;
        return;
    }

    m_pathValid = true;

    // 确保目录存在
    QFileInfo fileInfo(m_filePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            m_lastError = "Failed to create directory: " + dir.absolutePath();
            m_pathValid = false;
            return;
        }
    }

    // 打开文件
    if (!openFile()) {
        m_lastError = "Failed to open log file: " + m_filePath;
        m_pathValid = false;
        return;
    }

    // 启动异步写入线程
    if (m_async) {
        m_running.storeRelaxed(1);
        m_writerThread = QThread::create([this]() { writerThreadFunc(); });
        m_writerThread->start();
    }
}

FileAppender::~FileAppender() {
    close();
}

bool FileAppender::validateAndNormalizePath(const QString& filePath) {
    if (filePath.isEmpty()) {
        return false;
    }

    // 检查路径中的危险字符和模式
    QString cleanPath = filePath;

    // 检查路径遍历攻击模式
    if (cleanPath.contains("..") || cleanPath.contains("~")) {
        return false;
    }

    // 检查绝对路径是否指向系统敏感目录
    QFileInfo fileInfo(cleanPath);
    QString absolutePath = fileInfo.absoluteFilePath();

    // 规范化路径（解析符号链接等）
    QDir dir = fileInfo.absoluteDir();
    m_baseDir = dir.absolutePath();

    // 获取规范的绝对路径
    QString canonicalDir = dir.canonicalPath();
    if (!canonicalDir.isEmpty()) {
        m_baseDir = canonicalDir;
    }

    // 检查路径是否在允许的目录范围内
    if (!isPathInAllowedDir(absolutePath)) {
        return false;
    }

    // 存储规范化的路径
    m_filePath = absolutePath;
    m_canonicalPath = absolutePath;

    return true;
}

bool FileAppender::isPathInAllowedDir(const QString& path) const {
    // 允许的根目录列表
    QStringList allowedRoots;

    // 应用程序目录
    if (QCoreApplication::instance()) {
        allowedRoots << QCoreApplication::applicationDirPath();
    }

    // 用户数据目录
    allowedRoots << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    allowedRoots << QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    allowedRoots << QStandardPaths::writableLocation(QStandardPaths::TempLocation);

    // 用户主目录
    allowedRoots << QDir::homePath();

    // 当前工作目录
    allowedRoots << QDir::currentPath();

    // 检查路径是否在允许的根目录下
    for (const QString& root : allowedRoots) {
        if (path.startsWith(root)) {
            return true;
        }
    }

    return false;
}

void FileAppender::append(const LogRecord& record, const QString& formatted) {
    Q_UNUSED(record);

    if (!m_pathValid) {
        return;
    }

    if (m_async) {
        // 异步写入：加入队列（带大小限制）
        QMutexLocker locker(&m_queueMutex);

        // 检查队列大小
        if (m_maxQueueSize > 0 && m_writeQueue.size() >= m_maxQueueSize) {
            // 队列已满，设置溢出标记并丢弃最旧的消息
            m_queueOverflow.storeRelaxed(1);
            m_writeQueue.dequeue();
        }

        m_writeQueue.enqueue(formatted);
        m_queueCondition.wakeOne();
    } else {
        // 同步写入
        writeDirect(formatted);
    }
}

void FileAppender::flush() {
    if (!m_pathValid) {
        return;
    }

    if (m_async) {
        // 请求刷新
        m_flushRequested.storeRelaxed(1);
        m_queueCondition.wakeAll();

        // 等待队列清空（带超时保护）
        int timeout = 5000;  // 5秒超时
        int waited = 0;
        while (waited < timeout) {
            QMutexLocker locker(&m_queueMutex);
            if (m_writeQueue.isEmpty()) {
                break;
            }
            locker.unlock();
            QThread::msleep(10);
            waited += 10;
        }
    }

    // 刷新文件缓冲
    QMutexLocker fileLocker(&m_fileMutex);
    if (m_file.isOpen()) {
        m_file.flush();
    }
}

void FileAppender::close() {
    // 停止异步线程
    if (m_async && m_writerThread) {
        m_running.storeRelaxed(0);
        m_queueCondition.wakeAll();

        // 等待线程结束（带超时保护）
        if (!m_writerThread->wait(3000)) {
            m_writerThread->terminate();
            m_writerThread->wait();
        }
        delete m_writerThread;
        m_writerThread = nullptr;
    }

    // 关闭文件
    QMutexLocker locker(&m_fileMutex);
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }
}

bool FileAppender::openFile() {
    QMutexLocker locker(&m_fileMutex);

    if (m_file.isOpen()) {
        m_file.close();
    }

    m_file.setFileName(m_filePath);

    // 验证文件路径没有被篡改
    QFileInfo fileInfo(m_file);
    if (!isPathInAllowedDir(fileInfo.absoluteFilePath())) {
        return false;
    }

    return m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void FileAppender::rollOver() {
    // 使用递归锁，允许在 writeDirect 中调用
    QMutexLocker locker(&m_fileMutex);

    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }

    // 清理旧文件，为新文件腾出空间
    cleanupOldFiles();

    // 重命名现有文件
    QString rolledName = generateRolledFileName(1);

    // 确保目标文件不存在
    if (QFile::exists(rolledName)) {
        QFile::remove(rolledName);
    }

    if (!QFile::rename(m_filePath, rolledName)) {
        // 重命名失败，尝试直接删除并重新创建
        QFile::remove(m_filePath);
    }

    // 重新打开新文件
    m_file.setFileName(m_filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_pathValid = false;
        m_lastError = "Failed to reopen log file after rollover";
    }
}

void FileAppender::cleanupOldFiles() {
    // 注意：此函数在 rollOver 中调用，已持有 m_fileMutex
    QDir dir = QFileInfo(m_filePath).absoluteDir();
    QString baseName = QFileInfo(m_filePath).baseName();
    QString suffix = QFileInfo(m_filePath).completeSuffix();
    QString pattern = baseName + "_*";
    if (!suffix.isEmpty()) {
        pattern += "." + suffix;
    }

    QStringList filters;
    filters << pattern;
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);

    // 删除超出数量限制的旧文件
    while (files.size() >= m_maxFiles - 1) {
        QString toRemove = dir.absoluteFilePath(files.takeFirst());
        QFile::remove(toRemove);
    }
}

QString FileAppender::generateRolledFileName(int index) const {
    QFileInfo fileInfo(m_filePath);
    QString baseName = fileInfo.baseName();
    QString suffix = fileInfo.completeSuffix();
    QString path = fileInfo.absolutePath();

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString rolledName = QString("%1_%2_%3").arg(baseName, timestamp, QString::number(index));

    if (!suffix.isEmpty()) {
        rolledName += "." + suffix;
    }

    return QDir(path).absoluteFilePath(rolledName);
}

void FileAppender::writerThreadFunc() {
    while (m_running.loadRelaxed()) {
        QString message;
        bool overflowed = false;

        {
            QMutexLocker locker(&m_queueMutex);

            // 等待新消息或刷新请求
            while (m_writeQueue.isEmpty() && m_running.loadRelaxed() && !m_flushRequested.loadRelaxed()) {
                m_queueCondition.wait(&m_queueMutex, 100);
            }

            if (!m_writeQueue.isEmpty()) {
                message = m_writeQueue.dequeue();
            }

            // 检查是否有溢出发生过
            if (m_queueOverflow.loadRelaxed()) {
                overflowed = true;
                m_queueOverflow.storeRelaxed(0);
            }
        }

        // 如果发生过溢出，添加警告消息
        if (overflowed) {
            writeDirect("[WARNING] Log queue overflow occurred, some messages were dropped");
        }

        if (!message.isEmpty()) {
            writeDirect(message);
        }

        // 处理刷新请求
        if (m_flushRequested.loadRelaxed()) {
            QMutexLocker fileLocker(&m_fileMutex);
            if (m_file.isOpen()) {
                m_file.flush();
            }
            m_flushRequested.storeRelaxed(0);
        }
    }

    // 写入剩余消息
    while (true) {
        QString message;
        {
            QMutexLocker locker(&m_queueMutex);
            if (m_writeQueue.isEmpty()) {
                break;
            }
            message = m_writeQueue.dequeue();
        }

        if (!message.isEmpty()) {
            writeDirect(message);
        }
    }
}

void FileAppender::writeDirect(const QString& formatted) {
    QMutexLocker locker(&m_fileMutex);

    if (!m_pathValid) {
        return;
    }

    if (!m_file.isOpen()) {
        openFile();
    }

    if (m_file.isOpen()) {
        m_file.write(formatted.toUtf8());
        m_file.write("\n");

        // 检查是否需要滚动（避免在锁内调用 rollOver，因为它也使用同一个递归锁）
        if (needsRollover()) {
            locker.unlock();  // 释放锁
            rollOver();       // rollOver 会重新获取锁
        }
    }
}

bool FileAppender::needsRollover() const {
    return m_file.size() >= m_maxSize;
}

}  // namespace EasyKiConverter