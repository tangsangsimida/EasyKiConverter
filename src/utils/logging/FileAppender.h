#ifndef FILEAPPENDER_H
#define FILEAPPENDER_H

#include "IAppender.h"
#include "LogLevel.h"

#include <QFile>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <QQueue>
#include <QAtomicInt>

namespace EasyKiConverter {

/**
 * @brief 文件日志输出器
 * 
 * 支持以下特性：
 * - 按大小滚动（默认 10MB）
 * - 保留历史文件数量限制
 * - 异步写入（后台线程）
 * - 缓冲刷新
 */
class FileAppender : public IAppender {
public:
    /**
     * @brief 构造函数
     * @param filePath 日志文件路径
     * @param maxSize 最大文件大小（字节），超过此大小会滚动
     * @param maxFiles 最大保留文件数（包含当前文件）
     * @param async 是否启用异步写入
     */
    explicit FileAppender(const QString& filePath,
                          qint64 maxSize = 10 * 1024 * 1024,  // 10MB
                          int maxFiles = 5,
                          bool async = true);
    
    /**
     * @brief 析构函数
     */
    ~FileAppender() override;
    
    /**
     * @brief 输出日志到文件
     */
    void append(const LogRecord& record, const QString& formatted) override;
    
    /**
     * @brief 刷新缓冲区到文件
     */
    void flush() override;
    
    /**
     * @brief 关闭文件和后台线程
     */
    void close() override;
    
    /**
     * @brief 设置最大文件大小
     */
    void setMaxSize(qint64 maxSize) { m_maxSize = maxSize; }
    
    /**
     * @brief 获取最大文件大小
     */
    qint64 maxSize() const { return m_maxSize; }
    
    /**
     * @brief 设置最大文件数
     */
    void setMaxFiles(int maxFiles) { m_maxFiles = maxFiles; }
    
    /**
     * @brief 获取最大文件数
     */
    int maxFiles() const { return m_maxFiles; }
    
    /**
     * @brief 获取当前日志文件路径
     */
    QString filePath() const { return m_filePath; }
    
    /**
     * @brief 是否启用异步写入
     */
    bool isAsync() const { return m_async; }

private:
    QString m_filePath;
    qint64 m_maxSize;
    int m_maxFiles;
    bool m_async;
    
    QFile m_file;
    mutable QMutex m_fileMutex;
    
    // 异步写入相关
    QThread* m_writerThread = nullptr;
    QQueue<QString> m_writeQueue;
    mutable QMutex m_queueMutex;
    QWaitCondition m_queueCondition;
    QAtomicInt m_running;
    QAtomicInt m_flushRequested;
    
    /**
     * @brief 打开日志文件
     */
    bool openFile();
    
    /**
     * @brief 滚动日志文件
     */
    void rollOver();
    
    /**
     * @brief 清理旧日志文件
     */
    void cleanupOldFiles();
    
    /**
     * @brief 生成滚动文件名
     */
    QString generateRolledFileName(int index) const;
    
    /**
     * @brief 后台写入线程函数
     */
    void writerThreadFunc();
    
    /**
     * @brief 直接写入（同步模式或关闭时使用）
     */
    void writeDirect(const QString& formatted);
    
    /**
     * @brief 检查是否需要滚动
     */
    bool needsRollover() const;
};

} // namespace EasyKiConverter

#endif // FILEAPPENDER_H
