#include "ExportService.h"
#include <QDebug>
#include <QFile>
#include <QDir>
#include "src/core/kicad/ExporterSymbol.h"
#include "src/core/kicad/ExporterFootprint.h"
#include "src/core/kicad/Exporter3DModel.h"
#include "ComponentExportTask.h"

namespace EasyKiConverter
{

    ExportService::ExportService(QObject *parent)
        : QObject(parent), m_symbolExporter(new ExporterSymbol(this)), m_footprintExporter(new ExporterFootprint(this)), m_modelExporter(new Exporter3DModel(this)), m_threadPool(new QThreadPool(this)), m_mutex(new QMutex()), m_isExporting(false), m_currentProgress(0), m_totalProgress(0), m_successCount(0), m_failureCount(0), m_parallelExporting(false), m_parallelCompletedCount(0), m_parallelTotalCount(0)
    {
    }

    ExportService::~ExportService()
    {
    }

    bool ExportService::exportSymbol(const SymbolData &symbol, const QString &filePath)
    {
        qDebug() << "Exporting symbol to:" << filePath;

        // 检查文件是否已存在
        if (!m_options.overwriteExistingFiles && QFile::exists(filePath))
        {
            qWarning() << "Symbol file already exists:" << filePath;
            return false;
        }

        return m_symbolExporter->exportSymbol(symbol, filePath);
    }

    bool ExportService::exportFootprint(const FootprintData &footprint, const QString &filePath)
    {
        qDebug() << "Exporting footprint to:" << filePath;

        // 检查文件是否已存在
        if (!m_options.overwriteExistingFiles && QFile::exists(filePath))
        {
            qWarning() << "Footprint file already exists:" << filePath;
            return false;
        }

        return m_footprintExporter->exportFootprint(footprint, filePath);
    }

    bool ExportService::export3DModel(const Model3DData &model, const QString &filePath)
    {
        qDebug() << "Exporting 3D model to:" << filePath;

        // 检查文件是否已存在
        if (!m_options.overwriteExistingFiles && QFile::exists(filePath))
        {
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

        if (m_isExporting)
        {
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
        for (const QString &componentId : componentIds)
        {
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

        if (m_isExporting)
        {
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

        // 收集所有符号和封装数据
        QList<SymbolData> allSymbols;
        QList<FootprintData> allFootprints;
        QList<Model3DData> allModels;

        locker.unlock();

        // 为每个元件收集数据
        for (const ComponentData &componentData : componentDataList)
        {
            ExportData exportData;
            exportData.componentId = componentData.lcscId();

            // 检查并复制符号数据
            if (componentData.symbolData() && !componentData.symbolData()->info().name.isEmpty())
            {
                exportData.symbolData = *componentData.symbolData();
                allSymbols.append(exportData.symbolData);
            }

            // 检查并复制封装数据
            if (componentData.footprintData() && !componentData.footprintData()->info().name.isEmpty())
            {
                exportData.footprintData = *componentData.footprintData();
                allFootprints.append(exportData.footprintData);
            }

            // 检查并复制3D模型数据
            if (componentData.model3DData() && !componentData.model3DData()->uuid().isEmpty())
            {
                exportData.model3DData = *componentData.model3DData();
                allModels.append(exportData.model3DData);
            }

            exportData.success = false;
            m_exportDataList.append(exportData);

            qDebug() << "Added export data for:" << exportData.componentId
                     << "Symbol:" << !exportData.symbolData.info().name.isEmpty()
                     << "Footprint:" << !exportData.footprintData.info().name.isEmpty()
                     << "3D Model:" << !exportData.model3DData.uuid().isEmpty();
        }

        // 创建输出目录
        if (!createOutputDirectory(m_options.outputPath))
        {
            qWarning() << "Failed to create output directory:" << m_options.outputPath;
            m_isExporting = false;
            emit exportFailed("Failed to create output directory");
            return;
        }

        // 导出符号库
        if (m_options.exportSymbol && !allSymbols.isEmpty())
        {
            QString symbolLibPath = QString("%1/%2.kicad_sym").arg(m_options.outputPath, m_options.libName);
            if (exportSymbolLibrary(allSymbols, m_options.libName, symbolLibPath))
            {
                qDebug() << "Symbol library exported successfully:" << symbolLibPath;
                // 在追加模式下，如果所有符号都已存在，exportSymbolLibrary 会返回 true
                // 但实际上没有导出任何新符号，这种情况下我们认为这些符号已经存在
                // 不应该计入成功或失败计数
                // 只有在实际导出新符号或覆盖现有符号时才增加计数
                // 由于我们无法从返回值判断是否实际导出了新符号
                // 这里简单处理：如果文件存在，认为所有符号都已成功导出（包括之前已存在的）
                m_successCount += allSymbols.size();
            }
            else
            {
                qWarning() << "Failed to export symbol library";
                m_failureCount += allSymbols.size();
            }
        }

        // 导出3D模型（需要在导出封装库之前完成）
        QString modelsDirPath = QString("%1/3dmodels").arg(m_options.outputPath);
        if (m_options.exportModel3D && !allModels.isEmpty())
        {
            // 创建 3D 模型目录（添加库名称前缀）
            QString modelsDirPath = QString("%1/%2.3dmodels").arg(m_options.outputPath, m_options.libName);
            if (!createOutputDirectory(modelsDirPath))
            {
                qWarning() << "Failed to create 3D models directory:" << modelsDirPath;
            }
            else
            {
                // 为每个 3D 模型设置文件名（使用封装名称）
                QMap<QString, QString> modelPathMap; // UUID -> 文件路径映射

                for (auto &model : allModels)
                {
                    // 查找对应的封装名称
                    QString footprintName;
                    for (const auto &exportData : m_exportDataList)
                    {
                        if (exportData.model3DData.uuid() == model.uuid() && !exportData.footprintData.info().name.isEmpty())
                        {
                            footprintName = exportData.footprintData.info().name;
                            break;
                        }
                    }

                    // 使用封装名称作为文件名
                    QString modelName = footprintName.isEmpty() ? model.uuid() : footprintName;
                    QString wrlPath = QString("%1/%2.wrl").arg(modelsDirPath, modelName);
                    QString stepPath = QString("%1/%2.step").arg(modelsDirPath, modelName);

                    // 保存 WRL 模型
                    if (!model.rawObj().isEmpty())
                    {
                        if (!m_options.overwriteExistingFiles && QFile::exists(wrlPath))
                        {
                            qWarning() << "WRL model file already exists:" << wrlPath;
                        }
                        else
                        {
                            if (m_modelExporter->exportToWrl(model, wrlPath))
                            {
                                qDebug() << "WRL model exported successfully:" << wrlPath;
                            }
                            else
                            {
                                qWarning() << "Failed to export WRL model:" << modelName;
                            }
                        }
                    }

                    // 保存 STEP 模型
                    if (!model.step().isEmpty() && model.step().size() > 100)
                    {
                        if (!m_options.overwriteExistingFiles && QFile::exists(stepPath))
                        {
                            qWarning() << "STEP model file already exists:" << stepPath;
                        }
                        else
                        {
                            if (m_modelExporter->exportToStep(model, stepPath))
                            {
                                qDebug() << "STEP model exported successfully:" << stepPath;
                            }
                            else
                            {
                                qWarning() << "Failed to export STEP model:" << modelName;
                            }
                        }
                    }

                    // 保存模型路径映射（使用相对路径，相对于封装库目录）
                    QString relativePath = QString("${KIPRJMOD}/%1.3dmodels/%2").arg(m_options.libName, modelName);
                    modelPathMap[model.uuid()] = relativePath;
                }

                // 更新封装数据中的 3D 模型路径
                for (auto &footprint : allFootprints)
                {
                    if (!footprint.model3D().uuid().isEmpty() && modelPathMap.contains(footprint.model3D().uuid()))
                    {
                        Model3DData updatedModel = footprint.model3D();
                        updatedModel.setName(modelPathMap[footprint.model3D().uuid()]);
                        footprint.setModel3D(updatedModel);
                    }
                }
            }
        }

        // 创建封装库目录
        QString footprintDirPath = QString("%1/%2.pretty").arg(m_options.outputPath, m_options.libName);
        if (m_options.exportFootprint && !allFootprints.isEmpty())
        {
            if (exportFootprintLibrary(allFootprints, m_options.libName, footprintDirPath))
            {
                qDebug() << "Footprint library exported successfully with 3D model paths:" << footprintDirPath;
            }
            else
            {
                m_failureCount += allFootprints.size();
            }
        }

        // 更新进度和统计
        m_successCount = m_totalProgress - m_failureCount;
        m_currentProgress = m_totalProgress;

        // 发送导出完成信号
        emit exportProgress(m_currentProgress, m_totalProgress);

        // 为每个元件发送导出成功信号
        for (const ExportData &exportData : m_exportDataList)
        {
            emit componentExported(exportData.componentId, true, "Export successful");
        }

        // 完成导出
        m_isExporting = false;
        qDebug() << "Export pipeline completed:" << m_successCount << "success," << m_failureCount << "failed";
        emit exportCompleted(m_totalProgress, m_successCount);
    }

    void ExportService::cancelExport()
    {
        QMutexLocker locker(m_mutex);
        if (m_isExporting)
        {
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
        if (success)
        {
            m_successCount++;
        }
        else
        {
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
        bool fileExists = QFile::exists(filePath);
        bool appendMode = false;

        if (fileExists)
        {
            if (m_options.overwriteExistingFiles)
            {
                // 覆盖模式：删除现有文件
                qDebug() << "Overwriting existing symbol library:" << filePath;
                QFile::remove(filePath);
            }
            else
            {
                // 追加模式或更新模式：追加/更新新符号到现有库
                qDebug() << "Appending/Updating to existing symbol library:" << filePath;
                appendMode = true;
            }
        }

        return m_symbolExporter->exportSymbolLibrary(symbols, libName, filePath, appendMode, m_options.updateMode);
    }

    bool ExportService::exportFootprintLibrary(const QList<FootprintData> &footprints, const QString &libName, const QString &filePath)
    {
        qDebug() << "Exporting footprint library:" << libName << "with" << footprints.size() << "footprints";

        // 封装库是文件夹，即使文件夹存在也可以导出封装
        // ExporterFootprint 内部会处理覆盖逻辑
        return m_footprintExporter->exportFootprintLibrary(footprints, libName, filePath);
    }

    bool ExportService::export3DModels(const QList<Model3DData> &models, const QString &outputPath)
    {
        qDebug() << "Exporting" << models.size() << "3D models to:" << outputPath;

        // 创建输出目录
        if (!createOutputDirectory(outputPath))
        {
            return false;
        }

        bool success = true;
        for (const Model3DData &model : models)
        {
            QString modelPath = QString("%1/%2.wrl").arg(outputPath, model.uuid());

            // 检查文件是否已存在
            if (!m_options.overwriteExistingFiles && QFile::exists(modelPath))
            {
                qWarning() << "3D model file already exists:" << modelPath;
                continue;
            }

            if (!m_modelExporter->exportToWrl(model, modelPath))
            {
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

        if (!m_isExporting)
        {
            return;
        }

        // 检查是否所有元件都已处理
        if (m_currentProgress >= m_totalProgress)
        {
            m_isExporting = false;
            locker.unlock();

            qDebug() << "Export pipeline completed:" << m_successCount << "success," << m_failureCount << "failed";
            emit exportCompleted(m_totalProgress, m_successCount);
            return;
        }

        // 获取下一个待处理的元件
        int index = m_currentProgress;
        if (index >= m_exportDataList.size())
        {
            return;
        }

        ExportData &exportData = m_exportDataList[index];
        QString componentId = exportData.componentId;
        locker.unlock();

        // 检查是否有数据可以导出
        bool hasSymbolData = !exportData.symbolData.info().name.isEmpty();
        bool hasFootprintData = !exportData.footprintData.info().name.isEmpty();
        bool hasModel3DData = !exportData.model3DData.uuid().isEmpty();

        if (!hasSymbolData && !hasFootprintData && !hasModel3DData)
        {
            qWarning() << "No data available for export:" << componentId;
            handleExportTaskFinished(componentId, false, "No data available for export");
            return;
        }

        // 创建输出目录
        if (!createOutputDirectory(m_options.outputPath))
        {
            qWarning() << "Failed to create output directory:" << m_options.outputPath;
            handleExportTaskFinished(componentId, false, "Failed to create output directory");
            return;
        }

        // 导出符号
        if (m_options.exportSymbol && hasSymbolData)
        {
            QString symbolPath = QString("%1/%2.kicad_sym").arg(m_options.outputPath, componentId);
            if (!exportSymbol(exportData.symbolData, symbolPath))
            {
                qWarning() << "Failed to export symbol for:" << componentId;
                handleExportTaskFinished(componentId, false, "Failed to export symbol");
                return;
            }
        }

        // 导出封装
        if (m_options.exportFootprint && hasFootprintData)
        {
            QString footprintPath = QString("%1/%2.kicad_mod").arg(m_options.outputPath, componentId);
            if (!exportFootprint(exportData.footprintData, footprintPath))
            {
                qWarning() << "Failed to export footprint for:" << componentId;
                handleExportTaskFinished(componentId, false, "Failed to export footprint");
                return;
            }
        }

        // 导出3D模型
        if (m_options.exportModel3D && hasModel3DData)
        {
            QString modelPath = QString("%1/%2.wrl").arg(m_options.outputPath, exportData.model3DData.uuid());
            if (!export3DModel(exportData.model3DData, modelPath))
            {
                qWarning() << "Failed to export 3D model for:" << componentId;
                handleExportTaskFinished(componentId, false, "Failed to export 3D model");
                return;
            }
        }

        // 导出成功
        handleExportTaskFinished(componentId, true, "Export successful");
    }

    void ExportService::executeExportPipelineWithDataParallel(const QList<ComponentData> &componentDataList, const ExportOptions &options)
    {
        qDebug() << "Executing parallel export pipeline with data for" << componentDataList.size() << "components";

        QMutexLocker locker(m_mutex);

        if (m_isExporting)
        {
            qWarning() << "Export already in progress";
            return;
        }

        m_isExporting = true;
        m_parallelExporting = true;
        m_options = options;
        m_currentProgress = 0;
        m_totalProgress = componentDataList.size();
        m_successCount = 0;
        m_failureCount = 0;
        m_parallelCompletedCount = 0;
        m_parallelTotalCount = componentDataList.size();
        m_parallelExportStatus.clear();

        locker.unlock();

        // 发送导出开始信号
        // emit exportStarted(m_totalProgress); // 暂时注释掉，因为没有这个信号

        // 创建输出目录
        if (!createOutputDirectory(m_options.outputPath))
        {
            qWarning() << "Failed to create output directory:" << m_options.outputPath;
            m_isExporting = false;
            m_parallelExporting = false;
            emit exportFailed("Failed to create output directory");
            return;
        }

        // 收集所有符号、封装和3D模型数据
        QList<SymbolData> allSymbols;
        QList<FootprintData> allFootprints;
        QList<Model3DData> allModels;

        for (const auto &componentData : componentDataList)
        {
            if (componentData.symbolData())
            {
                allSymbols.append(*componentData.symbolData());
            }
            if (componentData.footprintData())
            {
                allFootprints.append(*componentData.footprintData());
            }
            if (componentData.model3DData())
            {
                allModels.append(*componentData.model3DData());
            }
        }

        // 导出符号库
        if (m_options.exportSymbol && !allSymbols.isEmpty())
        {
            QString symbolLibPath = QString("%1/%2.kicad_sym").arg(m_options.outputPath, m_options.libName);
            if (exportSymbolLibrary(allSymbols, m_options.libName, symbolLibPath))
            {
                qDebug() << "Symbol library exported successfully:" << symbolLibPath;
                // 在追加模式下，如果所有符号都已存在，exportSymbolLibrary 会返回 true
                // 但实际上没有导出任何新符号，这种情况下我们认为这些符号已经存在
                // 不应该计入成功或失败计数
                // 只有在实际导出新符号或覆盖现有符号时才增加计数
                // 由于我们无法从返回值判断是否实际导出了新符号
                // 这里简单处理：如果文件存在，认为所有符号都已成功导出（包括之前已存在的）
                m_successCount += allSymbols.size();
            }
            else
            {
                qWarning() << "Failed to export symbol library";
                m_failureCount += allSymbols.size();
            }
        }

        // 导出3D模型（需要在导出封装库之前完成）
        QString modelsDirPath = QString("%1/%2.3dmodels").arg(m_options.outputPath, m_options.libName);
        if (m_options.exportModel3D && !allModels.isEmpty())
        {
            // 创建 3D 模型目录
            if (!createOutputDirectory(modelsDirPath))
            {
                qWarning() << "Failed to create 3D models directory:" << modelsDirPath;
            }
            else
            {
                // 为每个 3D 模型设置文件名（使用封装名称）
                QMap<QString, QString> modelPathMap; // UUID -> 文件路径映射

                for (auto &model : allModels)
                {
                    // 查找对应的封装名称
                    QString footprintName;
                    for (const auto &componentData : componentDataList)
                    {
                        if (componentData.model3DData() && componentData.model3DData()->uuid() == model.uuid() &&
                            componentData.footprintData() && !componentData.footprintData()->info().name.isEmpty())
                        {
                            footprintName = componentData.footprintData()->info().name;
                            break;
                        }
                    }

                    // 使用封装名称作为文件名
                    QString modelName = footprintName.isEmpty() ? model.uuid() : footprintName;
                    QString wrlPath = QString("%1/%2.wrl").arg(modelsDirPath, modelName);
                    QString stepPath = QString("%1/%2.step").arg(modelsDirPath, modelName);

                    // 保存 WRL 模型
                    if (!model.rawObj().isEmpty())
                    {
                        if (!m_options.overwriteExistingFiles && QFile::exists(wrlPath))
                        {
                            qWarning() << "WRL model file already exists:" << wrlPath;
                        }
                        else
                        {
                            if (m_modelExporter->exportToWrl(model, wrlPath))
                            {
                                qDebug() << "WRL model exported successfully:" << wrlPath;
                            }
                            else
                            {
                                qWarning() << "Failed to export WRL model:" << modelName;
                            }
                        }
                    }

                    // 保存 STEP 模型
                    if (!model.step().isEmpty() && model.step().size() > 100)
                    {
                        if (!m_options.overwriteExistingFiles && QFile::exists(stepPath))
                        {
                            qWarning() << "STEP model file already exists:" << stepPath;
                        }
                        else
                        {
                            if (m_modelExporter->exportToStep(model, stepPath))
                            {
                                qDebug() << "STEP model exported successfully:" << stepPath;
                            }
                            else
                            {
                                qWarning() << "Failed to export STEP model:" << modelName;
                            }
                        }
                    }

                    // 保存模型路径映射（使用相对路径，相对于封装库目录）
                    QString relativePath = QString("${KIPRJMOD}/%1.3dmodels/%2").arg(m_options.libName, modelName);
                    modelPathMap[model.uuid()] = relativePath;
                }

                // 更新封装数据中的 3D 模型路径
                for (auto &footprint : allFootprints)
                {
                    if (!footprint.model3D().uuid().isEmpty() && modelPathMap.contains(footprint.model3D().uuid()))
                    {
                        Model3DData updatedModel = footprint.model3D();
                        updatedModel.setName(modelPathMap[footprint.model3D().uuid()]);
                        // 保留 STEP 数据
                        for (const auto &model : allModels)
                        {
                            if (model.uuid() == footprint.model3D().uuid())
                            {
                                updatedModel.setStep(model.step());
                                break;
                            }
                        }
                        footprint.setModel3D(updatedModel);
                    }
                }
            }
        }

        // 创建封装库目录
        QString footprintDirPath = QString("%1/%2.pretty").arg(m_options.outputPath, m_options.libName);
        if (m_options.exportFootprint && !allFootprints.isEmpty())
        {
            if (exportFootprintLibrary(allFootprints, m_options.libName, footprintDirPath))
            {
                qDebug() << "Footprint library exported successfully with 3D model paths:" << footprintDirPath;
                m_successCount += allFootprints.size();
            }
            else
            {
                m_failureCount += allFootprints.size();
            }
        }

        // 更新进度和统计
        m_currentProgress = m_totalProgress;

        // 发送导出完成信号
        m_isExporting = false;
        m_parallelExporting = false;
        emit exportProgress(m_currentProgress, m_totalProgress);
        emit exportCompleted(m_totalProgress, m_successCount);
    }

    void ExportService::handleParallelExportTaskFinished(const QString &componentId, bool success, const QString &message)
    {
        qDebug() << "Parallel export task finished for:" << componentId << "Success:" << success;

        QMutexLocker locker(m_mutex);

        // 更新统计信息
        if (success)
        {
            m_successCount++;
        }
        else
        {
            m_failureCount++;
        }

        // 更新状态
        m_parallelExportStatus[componentId] = false;
        m_parallelCompletedCount++;

        // 更新进度
        m_currentProgress++;
        locker.unlock();

        emit exportProgress(m_currentProgress, m_totalProgress);
        emit componentExported(componentId, success, message);

        // 检查是否所有任务都已完成
        if (m_parallelCompletedCount >= m_parallelTotalCount)
        {
            qDebug() << "All parallel export tasks completed:" << m_successCount << "success," << m_failureCount << "failed";

            m_isExporting = false;
            m_parallelExporting = false;
            emit exportCompleted(m_totalProgress, m_successCount);
        }
    }

} // namespace EasyKiConverter
