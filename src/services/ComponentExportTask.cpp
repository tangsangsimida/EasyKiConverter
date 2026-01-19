#include "ComponentExportTask.h"
#include "src/core/kicad/ExporterSymbol.h"
#include "src/core/kicad/ExporterFootprint.h"
#include "src/core/kicad/Exporter3DModel.h"
#include <QDebug>
#include <QFile>
#include <QDir>

namespace EasyKiConverter
{

ComponentExportTask::ComponentExportTask(const ComponentData &componentData,
                                         const ExportOptions &options,
                                         ExporterSymbol *symbolExporter,
                                         ExporterFootprint *footprintExporter,
                                         Exporter3DModel *modelExporter,
                                         QObject *parent)
    : QObject(parent)
    , QRunnable()
    , m_componentData(componentData)
    , m_options(options)
    , m_symbolExporter(symbolExporter)
    , m_footprintExporter(footprintExporter)
    , m_modelExporter(modelExporter)
{
    setAutoDelete(true);
}

ComponentExportTask::~ComponentExportTask()
{
}

void ComponentExportTask::run()
{
    qDebug() << "Running export task for:" << m_componentData.lcscId();
    
    QString componentId = m_componentData.lcscId();
    bool success = true;
    QString message;
    
    try {
        // 创建输出目录
        QDir dir;
        if (!dir.exists(m_options.outputPath)) {
            if (!dir.mkpath(m_options.outputPath)) {
                throw QString("Failed to create output directory");
            }
        }
        
        // 导出符号
        if (m_options.exportSymbol && m_componentData.symbolData() && !m_componentData.symbolData()->info().name.isEmpty()) {
            QString symbolPath = QString("%1/%2.kicad_sym").arg(m_options.outputPath, componentId);
            if (!m_options.overwriteExistingFiles && QFile::exists(symbolPath)) {
                qWarning() << "Symbol file already exists:" << symbolPath;
            } else {
                if (!m_symbolExporter->exportSymbol(*m_componentData.symbolData(), symbolPath)) {
                    throw QString("Failed to export symbol");
                }
                qDebug() << "Symbol exported successfully:" << symbolPath;
            }
        }
        
        // 导出3D模型（需要在导出封装之前完成）
        QString model3DPath;
        if (m_options.exportModel3D && m_componentData.model3DData() && !m_componentData.model3DData()->uuid().isEmpty()) {
            // 使用封装名称作为文件名
            QString footprintName = m_componentData.footprintData() ? m_componentData.footprintData()->info().name : m_componentData.model3DData()->name();
            if (footprintName.isEmpty()) {
                footprintName = m_componentData.model3DData()->uuid();
            }
            
            // 创建 3D 模型目录（添加库名称前缀）
            QString modelsDirPath = QString("%1/%2.3dmodels").arg(m_options.outputPath, m_options.libName);
            if (!dir.exists(modelsDirPath)) {
                if (!dir.mkpath(modelsDirPath)) {
                    throw QString("Failed to create 3D models directory");
                }
            }

            QString modelPath = QString("%1/%2.wrl").arg(modelsDirPath, footprintName);
            if (!m_options.overwriteExistingFiles && QFile::exists(modelPath)) {
                qWarning() << "3D model file already exists:" << modelPath;
            } else {
                if (!m_modelExporter->exportToWrl(*m_componentData.model3DData(), modelPath)) {
                    throw QString("Failed to export 3D model");
                }
                qDebug() << "3D model exported successfully:" << modelPath;
            }

            // 设置相对路径用于封装
            model3DPath = QString("${KIPRJMOD}/%1.3dmodels/%2").arg(m_options.libName, footprintName);
        }
        
        // 导出封装
        if (m_options.exportFootprint && m_componentData.footprintData() && !m_componentData.footprintData()->info().name.isEmpty()) {
            QString footprintPath = QString("%1/%2.kicad_mod").arg(m_options.outputPath, componentId);
            if (!m_options.overwriteExistingFiles && QFile::exists(footprintPath)) {
                qWarning() << "Footprint file already exists:" << footprintPath;
            } else {
                if (!m_footprintExporter->exportFootprint(*m_componentData.footprintData(), footprintPath, model3DPath)) {
                    throw QString("Failed to export footprint");
                }
                qDebug() << "Footprint exported successfully:" << footprintPath;
            }
        }
        
        message = "Export successful";
    } catch (const QString &error) {
        success = false;
        message = error;
        qWarning() << "Export task failed for:" << componentId << error;
    }
    
    emit exportFinished(componentId, success, message);
}

} // namespace EasyKiConverter