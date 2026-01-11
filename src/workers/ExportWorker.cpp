#include "ExportWorker.h"
#include "src/core/kicad/ExporterSymbol.h"
#include "src/core/kicad/ExporterFootprint.h"
#include "src/core/kicad/Exporter3DModel.h"
#include <QDir>
#include <QFile>
#include <QDebug>

namespace EasyKiConverter {

ExportWorker::ExportWorker(
    const QString &componentId,
    QSharedPointer<SymbolData> symbolData,
    QSharedPointer<FootprintData> footprintData,
    const QString &outputPath,
    const QString &libName,
    bool exportSymbol,
    bool exportFootprint,
    bool exportModel3D,
    int kicadVersion,
    QObject *parent)
    : QObject(parent)
    , QRunnable()
    , m_componentId(componentId)
    , m_symbolData(symbolData)
    , m_footprintData(footprintData)
    , m_outputPath(outputPath)
    , m_libName(libName)
    , m_exportSymbol(exportSymbol)
    , m_exportFootprint(exportFootprint)
    , m_exportModel3D(exportModel3D)
    , m_kicadVersion(kicadVersion)
{
    setAutoDelete(true); // 任务完成后自动删除
}

ExportWorker::~ExportWorker()
{
    qDebug() << "ExportWorker destroyed for:" << m_componentId;
}

void ExportWorker::run()
{
    qDebug() << "ExportWorker started for:" << m_componentId;

    try {
        // 创建输出目录
        if (!createOutputDirectories()) {
            emit exportFinished(m_componentId, false, "Failed to create output directories");
            return;
        }

        int progress = 0;
        int totalSteps = (m_exportSymbol ? 1 : 0) + (m_exportFootprint ? 1 : 0) + (m_exportModel3D ? 1 : 0);
        int currentStep = 0;

        // 导出符号
        if (m_exportSymbol) {
            qDebug() << "Exporting symbol for:" << m_componentId;
            if (!exportSymbolLibrary()) {
                emit exportFinished(m_componentId, false, "Failed to export symbol library");
                return;
            }
            currentStep++;
            progress = static_cast<int>((static_cast<double>(currentStep) / totalSteps) * 100);
            emit exportProgress(m_componentId, progress);
        }

        // 准备3D模型路径
        QString model3DPath;
        if (m_exportModel3D && m_footprintData && !m_footprintData->model3D().uuid().isEmpty()) {
            Model3DData model3D = m_footprintData->model3D();
            
            // 清理模型名称，移除文件系统不支持的字符
            QString sanitizedName = model3D.name();
            sanitizedName.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");
            
            // 计算3D模型导出路径
            model3DPath = QString("%1/%2.3dshapes/%3.wrl")
                              .arg(m_outputPath, m_libName, sanitizedName);
        }

        // 导出封装
        if (m_exportFootprint) {
            qDebug() << "Exporting footprint for:" << m_componentId;
            if (!exportFootprintLibrary(model3DPath)) {
                emit exportFinished(m_componentId, false, "Failed to export footprint library");
                return;
            }
            currentStep++;
            progress = static_cast<int>((static_cast<double>(currentStep) / totalSteps) * 100);
            emit exportProgress(m_componentId, progress);
        }

        // 导出3D模型
        if (m_exportModel3D && m_footprintData && !m_footprintData->model3D().uuid().isEmpty()) {
            qDebug() << "Exporting 3D model for:" << m_componentId;
            if (!export3DModel(m_footprintData->model3D(), model3DPath)) {
                emit exportFinished(m_componentId, false, "Failed to export 3D model");
                return;
            }
            currentStep++;
            progress = static_cast<int>((static_cast<double>(currentStep) / totalSteps) * 100);
            emit exportProgress(m_componentId, progress);
        }

        // 导出成功
        emit exportFinished(m_componentId, true, "Export successful");

    } catch (const std::exception &e) {
        qWarning() << "Exception in ExportWorker for" << m_componentId << ":" << e.what();
        emit exportFinished(m_componentId, false, QString("Exception: %1").arg(e.what()));
    } catch (...) {
        qWarning() << "Unknown exception in ExportWorker for:" << m_componentId;
        emit exportFinished(m_componentId, false, "Unknown exception occurred");
    }
}

bool ExportWorker::exportSymbolLibrary()
{
    try {
        QString symbolFilePath = QString("%1/%2.kicad_sym").arg(m_outputPath, m_libName);
        
        ExporterSymbol exporterSymbol;
        QList<SymbolData> symbolList;
        symbolList.append(*m_symbolData);
        
        bool success = exporterSymbol.exportSymbolLibrary(
            symbolList,
            m_libName,
            symbolFilePath,
            true  // appendMode
        );
        
        if (success) {
            qDebug() << "Symbol library exported successfully to:" << symbolFilePath;
        } else {
            qWarning() << "Failed to export symbol library to:" << symbolFilePath;
        }
        
        return success;
        
    } catch (const std::exception &e) {
        qWarning() << "Exception in exportSymbolLibrary:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "Unknown exception in exportSymbolLibrary";
        return false;
    }
}

bool ExportWorker::exportFootprintLibrary(const QString &model3DPath)
{
    try {
        QString footprintFilePath = QString("%1/%2.pretty/%3.kicad_mod")
                                     .arg(m_outputPath, m_libName, m_footprintData->info().name);
        
        ExporterFootprint exporterFootprint;
        
        bool success = exporterFootprint.exportFootprint(
            *m_footprintData,
            footprintFilePath,
            model3DPath
        );
        
        if (success) {
            qDebug() << "Footprint exported successfully to:" << footprintFilePath;
        } else {
            qWarning() << "Failed to export footprint to:" << footprintFilePath;
        }
        
        return success;
        
    } catch (const std::exception &e) {
        qWarning() << "Exception in exportFootprintLibrary:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "Unknown exception in exportFootprintLibrary";
        return false;
    }
}

bool ExportWorker::export3DModel(const Model3DData &model3DData, const QString &outputPath)
{
    try {
        Exporter3DModel exporter3DModel;
        
        bool success = exporter3DModel.exportToWrl(model3DData, outputPath);
        
        if (success) {
            qDebug() << "3D model exported successfully to:" << outputPath;
        } else {
            qWarning() << "Failed to export 3D model to:" << outputPath;
        }
        
        return success;
        
    } catch (const std::exception &e) {
        qWarning() << "Exception in export3DModel:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "Unknown exception in export3DModel";
        return false;
    }
}

bool ExportWorker::createOutputDirectories()
{
    try {
        QDir outputDir(m_outputPath);
        
        // 创建主输出目录
        if (!outputDir.exists()) {
            if (!outputDir.mkpath(".")) {
                qWarning() << "Failed to create output directory:" << m_outputPath;
                return false;
            }
        }
        
        // 创建符号库目录
        if (m_exportSymbol) {
            QString symbolDir = outputDir.filePath(m_libName);
            QDir dir(symbolDir);
            if (!dir.exists()) {
                if (!dir.mkpath(".")) {
                    qWarning() << "Failed to create symbol directory:" << symbolDir;
                    return false;
                }
            }
        }
        
        // 创建封装库目录
        if (m_exportFootprint) {
            QString footprintDir = outputDir.filePath(QString("%1.pretty").arg(m_libName));
            QDir dir(footprintDir);
            if (!dir.exists()) {
                if (!dir.mkpath(".")) {
                    qWarning() << "Failed to create footprint directory:" << footprintDir;
                    return false;
                }
            }
        }
        
        // 创建3D模型目录
        if (m_exportModel3D) {
            QString model3DDir = outputDir.filePath(QString("%1.3dshapes").arg(m_libName));
            QDir dir(model3DDir);
            if (!dir.exists()) {
                if (!dir.mkpath(".")) {
                    qWarning() << "Failed to create 3D model directory:" << model3DDir;
                    return false;
                }
            }
        }
        
        return true;
        
    } catch (const std::exception &e) {
        qWarning() << "Exception in createOutputDirectories:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "Unknown exception in createOutputDirectories";
        return false;
    }
}

} // namespace EasyKiConverter
