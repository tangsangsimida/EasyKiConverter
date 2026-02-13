#include "FileAppender.h"

#include <QDir>
#include <QFileInfo>
#include <QDateTime>

namespace EasyKiConverter {

FileAppender::FileAppender(const QString& filePath,
                           qint64 maxSize,
                           int maxFiles,
                           bool async)
    : m_filePath(filePath)
    , m_maxSize(maxSize)
    , m_maxFiles(qMax(1, maxFiles))
    , m_async(async)
{
    // 确保目录存在
    QFileInfo fileInfo(m_filePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // 打开文件
    openFile();
    
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

void FileAppender::append(const LogRecord& record, const QString& formatted) {
    Q_UNUSED(record);
    
    if (m_async) {
        // 异步写入：加入队列
        QMutexLocker locker(&m_queueMutex);
        m_writeQueue.enqueue(formatted);
        m_queueCondition.wakeOne();
    } else {
        // 同步写入
        writeDirect(formatted);
    }
}

void FileAppender::flush() {
    if (m_async) {
        // 请求刷新
        m_flushRequested.storeRelaxed(1);
        m_queueCondition.wakeAll();
        
        // 等待队列清空
        while (true) {
            QMutexLocker locker(&m_queueMutex);
            if (m_writeQueue.isEmpty()) {
                break;
            }
            locker.unlock();
            QThread::msleep(1);
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
        m_writerThread->wait();
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
    return m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void FileAppender::rollOver() {
    QMutexLocker locker(&m_fileMutex);
    
    if (m_file.isOpen()) {
        m_file.close();
    }
    
    // 清理旧文件，为新文件腾出空间
    cleanupOldFiles();
    
    // 重命名现有文件
    QString rolledName = generateRolledFileName(1);
    QFile::rename(m_filePath, rolledName);
    
    // 重新打开新文件
    m_file.setFileName(m_filePath);
    m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void FileAppender::cleanupOldFiles() {
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
        
        {
            QMutexLocker locker(&m_queueMutex);
            
            // 等待新消息或刷新请求
            while (m_writeQueue.isEmpty() && m_running.loadRelaxed() && !m_flushRequested.loadRelaxed()) {
                m_queueCondition.wait(&m_queueMutex, 100);
            }
            
            if (!m_writeQueue.isEmpty()) {
                message = m_writeQueue.dequeue();
            }
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
    
    if (!m_file.isOpen()) {
        openFile();
    }
    
    if (m_file.isOpen()) {
        m_file.write(formatted.toUtf8());
        m_file.write("\n");
        
        // 检查是否需要滚动
        if (needsRollover()) {
            locker.unlock();  // 释放锁以便 rollOver 可以获取它
            rollOver();
        }
    }
}

bool FileAppender::needsRollover() const {
    return m_file.size() >= m_maxSize;
}

} // namespace EasyKiConverter
