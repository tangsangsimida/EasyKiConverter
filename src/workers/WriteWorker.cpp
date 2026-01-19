#include "WriteWorker.h"
#include <QTextStream>
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
    if (m_exportModel3D && m_status.model3DData && !m_status.model3DData->rawObj().isEmpty()) {
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

    // 写入临时文件
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
    QString filePath = QString("%1/%2.kicad_mod").arg(footprintLibPath, status.componentId);
    QString model3DPath = m_exportModel3D ? QString("${KIPRJMOD}/%1.3dmodels/%2").arg(m_libName, status.componentId) : QString();

    if (!m_footprintExporter.exportFootprint(*status.footprintData, filePath, model3DPath)) {
        qWarning() << "Failed to write footprint file:" << filePath;
        return false;
    }

    qDebug() << "Footprint file written:" << filePath;
    return true;
}

bool WriteWorker::write3DModelFile(ComponentExportStatus &status)
{
    if (!status.model3DData || status.model3DData->rawObj().isEmpty()) {
        return true;
    }

    // 创建3D模型目录
    QString modelsDirPath = QString("%1/%2.3dmodels").arg(m_outputPath, m_libName);
    if (!createOutputDirectory(modelsDirPath)) {
        qWarning() << "Failed to create 3D models directory:" << modelsDirPath;
        return false;
    }

    // 写入OBJ文件
    QString objFilePath = QString("%1/%2.obj").arg(modelsDirPath, status.componentId);
    QFile objFile(objFilePath);
    if (!objFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open OBJ file for writing:" << objFilePath;
        return false;
    }

    QTextStream objStream(&objFile);
    objStream << status.model3DData->rawObj();
    objFile.close();

    qDebug() << "3D model file written:" << objFilePath;
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