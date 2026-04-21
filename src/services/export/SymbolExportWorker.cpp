#include "SymbolExportWorker.h"

#include "../../core/kicad/ExporterSymbol.h"
#include "../../models/ComponentData.h"
#include "DebugExportHelper.h"

#include <QDebug>
#include <QDir>
#include <QFile>

namespace EasyKiConverter {

SymbolExportWorker::SymbolExportWorker() : QObject(nullptr) {}

void SymbolExportWorker::setData(const QString& componentId,
                                 const QSharedPointer<ComponentData>& data,
                                 const struct ExportOptions& options) {
    m_componentId = componentId;
    m_data = data;
    m_options = options;
}

void SymbolExportWorker::setOptions(const struct ExportOptions& options) {
    m_options = options;
}

void SymbolExportWorker::run() {
    if (m_cancelled.load()) {
        emit completed(m_componentId, false, QStringLiteral("Cancelled"));
        return;
    }

    if (!m_data) {
        emit completed(m_componentId, false, QStringLiteral("No data available"));
        return;
    }

    qDebug() << "SymbolExportWorker: Exporting" << m_componentId;

    // 检查符号数据是否可用
    if (!m_data->symbolData()) {
        emit completed(m_componentId, false, QStringLiteral("Symbol data not available"));
        return;
    }

    // 构建输出路径
    QString outputDir = m_options.outputPath;
    if (outputDir.isEmpty()) {
        outputDir = QDir::currentPath() + QStringLiteral("/export/symbols");
    }

    QDir dir;
    if (!dir.mkpath(outputDir)) {
        emit completed(m_componentId, false, QStringLiteral("Failed to create output directory"));
        return;
    }

    QString fileName = m_componentId + QStringLiteral(".kicad_sym");
    QString filePath = outputDir + QStringLiteral("/") + fileName;

    // 检查文件是否已存在
    if (QFile::exists(filePath) && !m_options.overwriteExistingFiles) {
        qDebug() << "SymbolExportWorker: File already exists, skipping" << filePath;
        emit completed(m_componentId, true, QString());
        return;
    }

    // 执行符号导出
    try {
        ExporterSymbol exporter;
        bool success = exporter.exportSymbol(*m_data->symbolData(), filePath);

        if (m_cancelled.load()) {
            emit completed(m_componentId, false, QStringLiteral("Cancelled"));
            return;
        }

        if (success) {
            qDebug() << "SymbolExportWorker: Successfully exported" << filePath;

            // Debug 模式导出原始数据
            if (m_options.debugMode) {
                DebugExportHelper::exportDebugData(m_componentId, m_data, m_options.outputPath);
            }

            emit completed(m_componentId, true, QString());
        } else {
            emit completed(m_componentId, false, QStringLiteral("Failed to export symbol"));
        }

    } catch (const std::exception& e) {
        qCritical() << "SymbolExportWorker: Exception during export:" << e.what();
        emit completed(m_componentId, false, QString::fromUtf8(e.what()));
    } catch (...) {
        qCritical() << "SymbolExportWorker: Unknown exception during export";
        emit completed(m_componentId, false, QStringLiteral("Unknown error"));
    }
}

void SymbolExportWorker::cancel() {
    m_cancelled.store(true);
}

}  // namespace EasyKiConverter
