#include "DatasheetExportWorker.h"

#include "../../models/ComponentData.h"

#include <QDebug>
#include <QDir>
#include <QFile>

namespace EasyKiConverter {

DatasheetExportWorker::DatasheetExportWorker() : QObject(nullptr) {}

void DatasheetExportWorker::setData(const QString& componentId,
                                    const QSharedPointer<ComponentData>& data,
                                    const struct ExportOptions& options) {
    m_componentId = componentId;
    m_data = data;
    m_options = options;
}

void DatasheetExportWorker::setOptions(const struct ExportOptions& options) {
    m_options = options;
}

void DatasheetExportWorker::run() {
    if (m_cancelled.load()) {
        emit completed(m_componentId, false, QStringLiteral("Cancelled"));
        return;
    }

    if (!m_data) {
        emit completed(m_componentId, false, QStringLiteral("No data available"));
        return;
    }

    qDebug() << "DatasheetExportWorker: Exporting" << m_componentId;

    // 检查数据手册数据是否可用（URL或数据二选一）
    QString datasheetUrl = m_data->datasheet();
    QByteArray datasheetData = m_data->datasheetData();

    if (datasheetUrl.isEmpty() && datasheetData.isEmpty()) {
        emit completed(m_componentId, false, QStringLiteral("Datasheet not available"));
        return;
    }

    // 构建输出路径
    QString outputDir = m_options.outputPath;
    if (outputDir.isEmpty()) {
        outputDir = QDir::currentPath() + QStringLiteral("/export/datasheets");
    }

    QDir dir;
    if (!dir.mkpath(outputDir)) {
        emit completed(m_componentId, false, QStringLiteral("Failed to create output directory"));
        return;
    }

    // 从格式获取扩展名
    QString format = m_data->datasheetFormat();
    if (format.isEmpty()) {
        format = QStringLiteral("pdf");
    }

    QString fileName = m_componentId + QStringLiteral(".") + format;
    QString filePath = outputDir + QStringLiteral("/") + fileName;

    // 检查文件是否已存在
    if (QFile::exists(filePath) && !m_options.overwriteExistingFiles) {
        qDebug() << "DatasheetExportWorker: File already exists, skipping" << filePath;
        emit completed(m_componentId, true, QString());
        return;
    }

    // 执行数据手册导出
    try {
        if (m_cancelled.load()) {
            emit completed(m_componentId, false, QStringLiteral("Cancelled"));
            return;
        }

        // 优先使用缓存的数据，否则使用URL下载
        if (!datasheetData.isEmpty()) {
            // 使用内存中的数据写入文件
            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly)) {
                emit completed(m_componentId, false, QStringLiteral("Failed to open file for writing"));
                return;
            }

            qint64 written = file.write(datasheetData);
            file.close();

            if (written == datasheetData.size()) {
                qDebug() << "DatasheetExportWorker: Successfully exported" << filePath;
                emit completed(m_componentId, true, QString());
            } else {
                emit completed(m_componentId, false, QStringLiteral("Failed to write datasheet data"));
            }
        } else if (!datasheetUrl.isEmpty()) {
            // URL下载场景：数据手册导出需要从URL下载
            // 由于是QRunnable在子线程执行，不适合直接使用异步网络
            // 这里应该已经在预加载阶段完成了下载
            Q_UNUSED(datasheetUrl);
            emit completed(
                m_componentId, false, QStringLiteral("Datasheet URL download not implemented in export worker"));
        } else {
            emit completed(m_componentId, false, QStringLiteral("No datasheet data available"));
        }

    } catch (const std::exception& e) {
        qCritical() << "DatasheetExportWorker: Exception during export:" << e.what();
        emit completed(m_componentId, false, QString::fromUtf8(e.what()));
    } catch (...) {
        qCritical() << "DatasheetExportWorker: Unknown exception during export";
        emit completed(m_componentId, false, QStringLiteral("Unknown error"));
    }
}

void DatasheetExportWorker::cancel() {
    m_cancelled.store(true);
}

}  // namespace EasyKiConverter
