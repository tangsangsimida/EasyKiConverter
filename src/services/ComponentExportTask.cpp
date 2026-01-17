#include "ComponentExportTask.h"
#include "src/core/kicad/ExporterSymbol.h"
#include "src/core/kicad/ExporterFootprint.h"
#include "src/core/kicad/Exporter3DModel.h"
#include <QDir>
#include <QFile>
#include <QDebug>

namespace EasyKiConverter
{

    ComponentExportTask::ComponentExportTask(
        const QString &componentId,
        const ComponentData &componentData,
        const ExportOptions &options,
        QObject *parent)
        : QObject(parent), QRunnable(), m_componentId(componentId), m_componentData(componentData), m_options(options)
    {
        setAutoDelete(true); // 任务完成后自动删除
    }

    ComponentExportTask::~ComponentExportTask()
    {
        qDebug() << "ComponentExportTask destroyed for:" << m_componentId;
    }

    void ComponentExportTask::run()
    {
        qDebug() << "Starting export task for component:" << m_componentId;

        bool success = executeExport();

        QString message = success
                              ? "Export completed successfully"
                              : "Export failed";

        emit exportFinished(m_componentId, success, message);

        qDebug() << "Export task finished for component:" << m_componentId
                 << "- Success:" << success;
    }

    bool ComponentExportTask::executeExport()
    {
        // 创建输出目录
        if (!createOutputDirectory())
        {
            qWarning() << "Failed to create output directory for component:" << m_componentId;
            return false;
        }

        bool allSuccess = true;

        // 导出符号
        if (m_options.exportSymbol)
        {
            ExporterSymbol symbolExporter;
            QString symbolPath = getSymbolFilePath();

            if (!symbolExporter.exportSymbol(*m_componentData.symbolData(), symbolPath))
            {
                qWarning() << "Failed to export symbol for component:" << m_componentId;
                allSuccess = false;
            }
            else
            {
                qDebug() << "Symbol exported successfully:" << symbolPath;
            }
        }

        // 导出封装
        if (m_options.exportFootprint)
        {
            ExporterFootprint footprintExporter;
            QString footprintPath = getFootprintFilePath();

            if (!footprintExporter.exportFootprint(*m_componentData.footprintData(), footprintPath))
            {
                qWarning() << "Failed to export footprint for component:" << m_componentId;
                allSuccess = false;
            }
            else
            {
                qDebug() << "Footprint exported successfully:" << footprintPath;
            }
        }

        // 导出3D模型
        if (m_options.exportModel3D)
        {
            Exporter3DModel modelExporter;
            QString modelPath = getModel3DFilePath();

            if (!modelExporter.exportToWrl(*m_componentData.model3DData(), modelPath))
            {
                qWarning() << "Failed to export 3D model for component:" << m_componentId;
                // 3D模型导出失败不影响整体结果
            }
            else
            {
                qDebug() << "3D model exported successfully:" << modelPath;
            }
        }

        return allSuccess;
    }

    QString ComponentExportTask::getSymbolFilePath() const
    {
        QString dirPath = m_options.outputPath + "/symbols";
        QDir dir(dirPath);

        if (!dir.exists())
        {
            dir.mkpath(".");
        }

        return dirPath + "/" + m_options.libName + ".kicad_sym";
    }

    QString ComponentExportTask::getFootprintFilePath() const
    {
        QString dirPath = m_options.outputPath + "/footprints";
        QDir dir(dirPath);

        if (!dir.exists())
        {
            dir.mkpath(".");
        }

        return dirPath + "/" + m_options.libName + ".pretty/" + m_componentId + ".kicad_mod";
    }

    QString ComponentExportTask::getModel3DFilePath() const
    {
        QString dirPath = m_options.outputPath + "/3D";
        QDir dir(dirPath);

        if (!dir.exists())
        {
            dir.mkpath(".");
        }

        const Model3DData &model3D = *m_componentData.model3DData();
        QString uuid = model3D.uuid();

        if (uuid.isEmpty())
        {
            return QString();
        }

        return dirPath + "/" + uuid + ".step";
    }

    bool ComponentExportTask::createOutputDirectory() const
    {
        QDir dir(m_options.outputPath);

        if (!dir.exists())
        {
            if (!dir.mkpath("."))
            {
                qWarning() << "Failed to create output directory:" << m_options.outputPath;
                return false;
            }
        }

        return true;
    }

} // namespace EasyKiConverter
