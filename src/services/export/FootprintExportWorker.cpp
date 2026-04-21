#include "FootprintExportWorker.h"

#include "../../core/kicad/ExporterFootprint.h"
#include "../../models/ComponentData.h"
#include "DebugExportHelper.h"

#include <QDebug>
#include <QDir>
#include <QFile>

namespace EasyKiConverter {

FootprintExportWorker::FootprintExportWorker() : QObject(nullptr) {}

void FootprintExportWorker::setData(const QString& componentId,
                                    const QSharedPointer<ComponentData>& data,
                                    const struct ExportOptions& options) {
    m_componentId = componentId;
    m_data = data;
    m_options = options;
}

void FootprintExportWorker::setOptions(const struct ExportOptions& options) {
    m_options = options;
}

void FootprintExportWorker::run() {
    if (m_cancelled.load()) {
        emit completed(m_componentId, false, QStringLiteral("Cancelled"));
        return;
    }

    if (!m_data) {
        emit completed(m_componentId, false, QStringLiteral("No data available"));
        return;
    }

    qDebug() << "FootprintExportWorker: Exporting" << m_componentId;

    // 检查封装数据是否可用
    if (!m_data->footprintData()) {
        emit completed(m_componentId, false, QStringLiteral("Footprint data not available"));
        return;
    }

    // 构建输出路径
    QString outputDir = m_options.outputPath;
    if (outputDir.isEmpty()) {
        outputDir = QDir::currentPath() + QStringLiteral("/export/footprints");
    }

    QDir dir;
    if (!dir.mkpath(outputDir)) {
        emit completed(m_componentId, false, QStringLiteral("Failed to create output directory"));
        return;
    }

    QString fileName = m_componentId + QStringLiteral(".kicad_mod");
    QString filePath = outputDir + QStringLiteral("/") + fileName;

    // 检查文件是否已存在
    if (QFile::exists(filePath) && !m_options.overwriteExistingFiles) {
        qDebug() << "FootprintExportWorker: File already exists, skipping" << filePath;
        emit completed(m_componentId, true, QString());
        return;
    }

    // 执行封装导出
    try {
        ExporterFootprint exporter;
        // 3D模型路径为空字符串表示不导出3D模型
        bool success = exporter.exportFootprint(*m_data->footprintData(), filePath, QString());

        if (m_cancelled.load()) {
            emit completed(m_componentId, false, QStringLiteral("Cancelled"));
            return;
        }

        if (success) {
            qDebug() << "FootprintExportWorker: Successfully exported" << filePath;

            // Debug 模式导出原始数据
            if (m_options.debugMode) {
                DebugExportHelper::exportDebugData(m_componentId, m_data, m_options.outputPath);
            }

            emit completed(m_componentId, true, QString());
        } else {
            emit completed(m_componentId, false, QStringLiteral("Failed to export footprint"));
        }

    } catch (const std::exception& e) {
        qCritical() << "FootprintExportWorker: Exception during export:" << e.what();
        emit completed(m_componentId, false, QString::fromUtf8(e.what()));
    } catch (...) {
        qCritical() << "FootprintExportWorker: Unknown exception during export";
        emit completed(m_componentId, false, QStringLiteral("Unknown error"));
    }
}

void FootprintExportWorker::cancel() {
    m_cancelled.store(true);
}

}  // namespace EasyKiConverter
