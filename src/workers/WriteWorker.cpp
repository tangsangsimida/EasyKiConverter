#include "WriteWorker.h"
#include "src/core/kicad/ExporterSymbol.h"
#include "src/core/kicad/ExporterFootprint.h"
#include "src/core/kicad/Exporter3DModel.h"
#include <QDir>
#include <QFile>
#include <QDebug>

namespace EasyKiConverter {

WriteWorker::WriteWorker(
    const ComponentExportStatus &status,
    const QString &outputPath,
    const QString &libName,
    bool exportSymbol,
    bool exportFootprint,
    bool exportModel3D,
    QObject *parent)
    : QObject(parent)
    , m_status(status)
    , m_outputPath(outputPath)
    , m_libName(libName)
    , m_exportSymbol(exportSymbol)
    , m_exportFootprint(exportFootprint)
    , m_exportModel3D(exportModel3D)
    , m_symbolExporter()
    , m_footprintExporter()
    , m_model3DExporter()
{
}

WriteWorker::~WriteWorker()
{
}

void WriteWorker::run()
{
    qDebug() << "WriteWorker started for component:" << m_status.componentId;

    // 创建输出目录
    if (!createOutputDirectory(m_outputPath)) {
        m_status.writeSuccess = false;
        m_status.writeMessage = "Failed to create output directory";
        emit writeCompleted(m_status);
        return;
    }

    // 写入符号文件
    if (m_exportSymbol && m_status.symbolData) {
        if (!writeSymbolFile(m_status)) {
            m_status.writeSuccess = false;
            m_status.writeMessage = "Failed to write symbol file";
            emit writeCompleted(m_status);
            return;
        }
    }

    // 写入封装文件
    if (m_exportFootprint && m_status.footprintData) {
        if (!writeFootprintFile(m_status)) {
            m_status.writeSuccess = false;
            m_status.writeMessage = "Failed to write footprint file";
            emit writeCompleted(m_status);
            return;
        }
    }

    // 写入3D模型文件
    if (m_exportModel3D && m_status.model3DData && !m_status.model3DObjRaw.isEmpty()) {
        if (!write3DModelFile(m_status)) {
            m_status.writeSuccess = false;
            m_status.writeMessage = "Failed to write 3D model file";
            emit writeCompleted(m_status);
            return;
        }
    }

    m_status.writeSuccess = true;
    m_status.writeMessage = "Write completed successfully";
    qDebug() << "WriteWorker completed successfully for component:" << m_status.componentId;

    emit writeCompleted(m_status);
}

bool WriteWorker::writeSymbolFile(ComponentExportStatus &status)
{
    if (!status.symbolData) {
        return true;
    }

    // 创建临时符号文件
    QString tempFilePath = QString("%1/%2.kicad_sym.tmp").arg(m_outputPath, status.componentId);
    
    if (!m_symbolExporter.exportSymbol(*status.symbolData, tempFilePath)) {
        qWarning() << "Failed to write symbol file:" << tempFilePath;
        return false;
    }

    qDebug() << "Symbol file written:" << tempFilePath;
    return true;
}

bool WriteWorker::writeFootprintFile(ComponentExportStatus &status)
{
    if (!status.footprintData) {
        return true;
    }

    // 创建封装库目录
    QString footprintLibPath = QString("%1/%2.pretty").arg(m_outputPath, m_libName);
    if (!createOutputDirectory(footprintLibPath)) {
        qWarning() << "Failed to create footprint library directory:" << footprintLibPath;
        return false;
    }

    // 创建3D模型目录
    QString modelsDirPath = QString("%1/%2.3dmodels").arg(m_outputPath, m_libName);
    if (m_exportModel3D) {
        createOutputDirectory(modelsDirPath);
    }

    // 写入封装文件
    QString footprintName = status.footprintData->info().name;
    QString filePath = QString("%1/%2.kicad_mod").arg(footprintLibPath, footprintName);
    
    // 准备3D模型路径
    QString model3DWrlPath;
    QString model3DStepPath;
    if (m_exportModel3D && status.model3DData && !status.model3DData->uuid().isEmpty()) {
        model3DWrlPath = QString("${KIPRJMOD}/%1.3dmodels/%2.wrl")
                           .arg(m_libName, footprintName);
        
        if (!status.model3DStepRaw.isEmpty()) {
            model3DStepPath = QString("${KIPRJMOD}/%1.3dmodels/%2.step")
                               .arg(m_libName, footprintName);
        }
    }

    // 使用两个3D模型路径导出封装
    if (!model3DStepPath.isEmpty()) {
        if (!m_footprintExporter.exportFootprint(*status.footprintData, filePath, model3DWrlPath, model3DStepPath)) {
            qWarning() << "Failed to write footprint file:" << filePath;
            return false;
        }
    } else {
        if (!m_footprintExporter.exportFootprint(*status.footprintData, filePath, model3DWrlPath)) {
            qWarning() << "Failed to write footprint file:" << filePath;
            return false;
        }
    }

    qDebug() << "Footprint file written:" << filePath;
    return true;
}

bool WriteWorker::write3DModelFile(ComponentExportStatus &status)
{
    if (!status.model3DData || status.model3DObjRaw.isEmpty()) {
        return true;
    }

    // 创建3D模型目录
    QString modelsDirPath = QString("%1/%2.3dmodels").arg(m_outputPath, m_libName);
    if (!createOutputDirectory(modelsDirPath)) {
        qWarning() << "Failed to create 3D models directory:" << modelsDirPath;
        return false;
    }

    // 使用封装名称作为文件名
    QString footprintName = status.footprintData ? status.footprintData->info().name : status.componentId;
    
    // 写入WRL文件
    QString wrlFilePath = QString("%1/%2.wrl").arg(modelsDirPath, footprintName);
    if (!m_model3DExporter.exportToWrl(*status.model3DData, wrlFilePath)) {
        qWarning() << "Failed to write WRL file:" << wrlFilePath;
    } else {
        qDebug() << "3D model WRL file written:" << wrlFilePath;
    }

    // 写入STEP文件（如果有）
    if (!status.model3DStepRaw.isEmpty()) {
        QString stepFilePath = QString("%1/%2.step").arg(modelsDirPath, footprintName);
        QFile stepFile(stepFilePath);
        if (stepFile.open(QIODevice::WriteOnly)) {
            stepFile.write(status.model3DStepRaw);
            stepFile.close();
            qDebug() << "3D model STEP file written:" << stepFilePath;
        } else {
            qWarning() << "Failed to write STEP file:" << stepFilePath;
        }
    }

    return true;
}

bool WriteWorker::createOutputDirectory(const QString &path)
{
    QDir dir;
    if (!dir.exists(path)) {
        if (!dir.mkpath(path)) {
            qWarning() << "Failed to create directory:" << path;
            return false;
        }
    }
    return true;
}

} // namespace EasyKiConverter