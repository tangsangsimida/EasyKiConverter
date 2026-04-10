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
    QString finalPath = outputDir + QStringLiteral("/") + fileName;

    // 使用temp路径如果可用
    QString filePath = m_tempPath.isEmpty() ? finalPath : m_tempPath;

    // 检查文件是否已存在（仅对最终路径且非temp模式）
    if (filePath == finalPath && QFile::exists(finalPath) && !m_options.overwriteExistingFiles) {
        qDebug() << "DatasheetExportWorker: File already exists, skipping" << finalPath;
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
            // URL存在但数据为空：说明预加载阶段的数据手册下载未完成或失败
            // 这种情况不应该发生，因为预加载应该等待所有异步下载完成
            qWarning() << "DatasheetExportWorker: URL exists but data is empty for" << m_componentId
                       << "- preload phase did not complete datasheet download";
            emit completed(
                m_componentId, false, QStringLiteral("Datasheet download incomplete (preload phase did not complete)"));
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
