#include "ExportService.h"
#include <QDebug>
#include <QFile>
#include <QDir>
#include "src/core/kicad/ExporterSymbol.h"
#include "src/core/kicad/ExporterFootprint.h"
#include "src/core/kicad/Exporter3DModel.h"

namespace EasyKiConverter
{

ExportService::ExportService(QObject *parent)
    : QObject(parent)
    , m_symbolExporter(new ExporterSymbol(this))
    , m_footprintExporter(new ExporterFootprint(this))
    , m_modelExporter(new Exporter3DModel(this))
    , m_threadPool(new QThreadPool(this))
    , m_mutex(new QMutex())
    , m_isExporting(false)
    , m_currentProgress(0)
    , m_totalProgress(0)
    , m_successCount(0)
    , m_failureCount(0)
{
}

ExportService::~ExportService()
{
}

bool ExportService::exportSymbol(const SymbolData &symbol, const QString &filePath)
{
    qDebug() << "Exporting symbol to:" << filePath;
    
    // 检查文件是否已存在
    if (!m_options.overwriteExistingFiles && QFile::exists(filePath)) {
        qWarning() << "Symbol file already exists:" << filePath;
        return false;
    }
    
    return m_symbolExporter->exportSymbol(symbol, filePath);
}

bool ExportService::exportFootprint(const FootprintData &footprint, const QString &filePath)
{
    qDebug() << "Exporting footprint to:" << filePath;
    
    // 检查文件是否已存在
    if (!m_options.overwriteExistingFiles && QFile::exists(filePath)) {
        qWarning() << "Footprint file already exists:" << filePath;
        return false;
    }
    
    return m_footprintExporter->exportFootprint(footprint, filePath);
}

bool ExportService::export3DModel(const Model3DData &model, const QString &filePath)
{
    qDebug() << "Exporting 3D model to:" << filePath;
    
    // 检查文件是否已存在
    if (!m_options.overwriteExistingFiles && QFile::exists(filePath)) {
        qWarning() << "3D model file already exists:" << filePath;
        return false;
    }
    
    // 使用 Exporter3DModel 导出为 WRL 格式
    return m_modelExporter->exportToWrl(model, filePath);
}

void ExportService::executeExportPipeline(const QStringList &componentIds, const ExportOptions &options)
{
    qDebug() << "Executing export pipeline for" << componentIds.size() << "components";
    
    QMutexLocker locker(m_mutex);
    
    if (m_isExporting) {
        qWarning() << "Export already in progress";
        return;
    }
    
    m_isExporting = true;
    m_options = options;
    m_currentProgress = 0;
    m_totalProgress = componentIds.size();
    m_successCount = 0;
    m_failureCount = 0;
    
    // 清空之前的数据
    m_exportDataList.clear();
    
    locker.unlock();
    
    // 为每个元件创建导出任务
    for (const QString &componentId : componentIds) {
        ExportData exportData;
        exportData.componentId = componentId;
        exportData.success = false;
        m_exportDataList.append(exportData);
    }
    
    // 开始处理第一个元件
    processNextExport();
}

void ExportService::executeExportPipelineWithData(const QList<ComponentData> &componentDataList, const ExportOptions &options)
{
    qDebug() << "Executing export pipeline with data for" << componentDataList.size() << "components";
    
    QMutexLocker locker(m_mutex);
    
    if (m_isExporting) {
        qWarning() << "Export already in progress";
        return;
    }
    
    m_isExporting = true;
    m_options = options;
    m_currentProgress = 0;
    m_totalProgress = componentDataList.size();
    m_successCount = 0;
    m_failureCount = 0;
    
    // 清空之前的数据
    m_exportDataList.clear();
    
    locker.unlock();
    
    // 为每个元件创建导出任务，并填充数据
    for (const ComponentData &componentData : componentDataList) {
        ExportData exportData;
        exportData.componentId = componentData.lcscId();
        
        // 检查并复制符号数据
        if (componentData.symbolData() && !componentData.symbolData()->info().name.isEmpty()) {
            exportData.symbolData = *componentData.symbolData();
        }
        
        // 检查并复制封装数据
        if (componentData.footprintData() && !componentData.footprintData()->info().name.isEmpty()) {
            exportData.footprintData = *componentData.footprintData();
        }
        
        // 检查并复制3D模型数据
        if (componentData.model3DData() && !componentData.model3DData()->uuid().isEmpty()) {
            exportData.model3DData = *componentData.model3DData();
        }
        
        exportData.success = false;
        m_exportDataList.append(exportData);
        
        qDebug() << "Added export data for:" << exportData.componentId 
                 << "Symbol:" << !exportData.symbolData.info().name.isEmpty()
                 << "Footprint:" << !exportData.footprintData.info().name.isEmpty()
                 << "3D Model:" << !exportData.model3DData.uuid().isEmpty();
    }
    
    // 开始处理第一个元件
    processNextExport();
}

void ExportService::cancelExport()
{
    QMutexLocker locker(m_mutex);
    if (m_isExporting) {
        qDebug() << "Canceling export";
        m_isExporting = false;
        emit exportFailed("Export cancelled");
    }
}

void ExportService::setExportOptions(const ExportOptions &options)
{
    QMutexLocker locker(m_mutex);
    m_options = options;
}

ExportOptions ExportService::getExportOptions() const
{
    QMutexLocker locker(m_mutex);
    return m_options;
}

bool ExportService::isExporting() const
{
    return m_isExporting;
}

void ExportService::handleExportTaskFinished(const QString &componentId, bool success, const QString &message)
{
    qDebug() << "Export task finished for:" << componentId << "Success:" << success;
    
    QMutexLocker locker(m_mutex);
    
    // 更新统计信息
    if (success) {
        m_successCount++;
    } else {
        m_failureCount++;
    }
    
    // 更新进度
    m_currentProgress++;
    locker.unlock();
    
    emit exportProgress(m_currentProgress, m_totalProgress);
    emit componentExported(componentId, success, message);
    
    // 处理下一个元件
    processNextExport();
}

bool ExportService::exportSymbolLibrary(const QList<SymbolData> &symbols, const QString &libName, const QString &filePath)
{
    qDebug() << "Exporting symbol library:" << libName << "with" << symbols.size() << "symbols";
    
    // 检查文件是否已存在
    if (!m_options.overwriteExistingFiles && QFile::exists(filePath)) {
        qWarning() << "Symbol library file already exists:" << filePath;
        return false;
    }
    
    return m_symbolExporter->exportSymbolLibrary(symbols, libName, filePath);
}

bool ExportService::exportFootprintLibrary(const QList<FootprintData> &footprints, const QString &libName, const QString &filePath)
{
    qDebug() << "Exporting footprint library:" << libName << "with" << footprints.size() << "footprints";
    
    // 检查文件是否已存在
    if (!m_options.overwriteExistingFiles && QFile::exists(filePath)) {
        qWarning() << "Footprint library file already exists:" << filePath;
        return false;
    }
    
    return m_footprintExporter->exportFootprintLibrary(footprints, libName, filePath);
}

bool ExportService::export3DModels(const QList<Model3DData> &models, const QString &outputPath)
{
    qDebug() << "Exporting" << models.size() << "3D models to:" << outputPath;
    
    // 创建输出目录
    if (!createOutputDirectory(outputPath)) {
        return false;
    }
    
    bool success = true;
    for (const Model3DData &model : models) {
        QString modelPath = QString("%1/%2.wrl").arg(outputPath, model.uuid());
        
        // 检查文件是否已存在
        if (!m_options.overwriteExistingFiles && QFile::exists(modelPath)) {
            qWarning() << "3D model file already exists:" << modelPath;
            continue;
        }
        
        if (!m_modelExporter->exportToWrl(model, modelPath)) {
            qWarning() << "Failed to export 3D model:" << model.uuid();
            success = false;
        }
    }
    
    return success;
}

void ExportService::updateProgress(int current, int total)
{
    m_currentProgress = current;
    m_totalProgress = total;
    emit exportProgress(current, total);
}

bool ExportService::fileExists(const QString &filePath) const
{
    return QFile::exists(filePath);
}

bool ExportService::createOutputDirectory(const QString &path) const
{
    QDir dir;
    return dir.mkpath(path);
}

void ExportService::processNextExport()
{
    QMutexLocker locker(m_mutex);
    
    if (!m_isExporting) {
        return;
    }
    
    // 检查是否所有元件都已处理
    if (m_currentProgress >= m_totalProgress) {
        m_isExporting = false;
        locker.unlock();
        
        qDebug() << "Export pipeline completed:" << m_successCount << "success," << m_failureCount << "failed";
        emit exportCompleted(m_totalProgress, m_successCount);
        return;
    }
    
    // 获取下一个待处理的元件
    int index = m_currentProgress;
    if (index >= m_exportDataList.size()) {
        return;
    }
    
    ExportData &exportData = m_exportDataList[index];
    QString componentId = exportData.componentId;
    locker.unlock();
    
    // 检查是否有数据可以导出
    bool hasSymbolData = !exportData.symbolData.info().name.isEmpty();
    bool hasFootprintData = !exportData.footprintData.info().name.isEmpty();
    bool hasModel3DData = !exportData.model3DData.uuid().isEmpty();
    
    if (!hasSymbolData && !hasFootprintData && !hasModel3DData) {
        qWarning() << "No data available for export:" << componentId;
        handleExportTaskFinished(componentId, false, "No data available for export");
        return;
    }
    
    // 创建输出目录
    if (!createOutputDirectory(m_options.outputPath)) {
        qWarning() << "Failed to create output directory:" << m_options.outputPath;
        handleExportTaskFinished(componentId, false, "Failed to create output directory");
        return;
    }
    
    // 导出符号
    if (m_options.exportSymbol && hasSymbolData) {
        QString symbolPath = QString("%1/%2.kicad_sym").arg(m_options.outputPath, componentId);
        if (!exportSymbol(exportData.symbolData, symbolPath)) {
            qWarning() << "Failed to export symbol for:" << componentId;
            handleExportTaskFinished(componentId, false, "Failed to export symbol");
            return;
        }
    }
    
    // 导出封装
    if (m_options.exportFootprint && hasFootprintData) {
        QString footprintPath = QString("%1/%2.kicad_mod").arg(m_options.outputPath, componentId);
        if (!exportFootprint(exportData.footprintData, footprintPath)) {
            qWarning() << "Failed to export footprint for:" << componentId;
            handleExportTaskFinished(componentId, false, "Failed to export footprint");
            return;
        }
    }
    
    // 导出3D模型
    if (m_options.exportModel3D && hasModel3DData) {
        QString modelPath = QString("%1/%2.wrl").arg(m_options.outputPath, exportData.model3DData.uuid());
        if (!export3DModel(exportData.model3DData, modelPath)) {
            qWarning() << "Failed to export 3D model for:" << componentId;
            handleExportTaskFinished(componentId, false, "Failed to export 3D model");
            return;
        }
    }
    
    // 导出成功
    handleExportTaskFinished(componentId, true, "Export successful");
}

} // namespace EasyKiConverter