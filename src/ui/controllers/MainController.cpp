#include "MainController.h"
#include <QMessageBox>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDir>
#include <QTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QClipboard>
#include <zlib.h>
#include "src/core/kicad/ExporterSymbol.h"
#include "src/core/kicad/ExporterFootprint.h"
#include "src/core/kicad/Exporter3DModel.h"

namespace EasyKiConverter {

MainController::MainController(QObject *parent)
    : QObject(parent)
    , m_configManager(new ConfigManager(this))
    , m_exportSymbol(true)
    , m_exportFootprint(true)
    , m_exportModel3D(true)
    , m_progress(0)
    , m_isExporting(false)
    , m_isDarkMode(false)
    , m_currentComponentIndex(0)
    , m_successCount(0)
    , m_failureCount(0)
    , m_pending3DModel(nullptr)
    , m_objDownloaded(false)
    , m_threadPool(new QThreadPool(this))
    , m_mutex(new QMutex())
    , m_useParallelExport(true)
{
    // 初始化核心转换引擎
    m_easyedaApi = new EasyedaApi(this);
    m_easyedaImporter = new EasyedaImporter(this);
    m_exporterSymbol = new ExporterSymbol(this);
    m_exporterFootprint = new ExporterFootprint(this);
    m_exporter3DModel = new Exporter3DModel(this);

    // 连接 EasyedaApi 的信号
    connect(m_easyedaApi, &EasyedaApi::componentInfoFetched, this, &MainController::handleComponentInfoFetched);
    connect(m_easyedaApi, &EasyedaApi::cadDataFetched, this, &MainController::handleCadDataFetched);
    connect(m_easyedaApi, &EasyedaApi::model3DFetched, this, &MainController::handleModel3DFetched);
    connect(m_easyedaApi, &EasyedaApi::fetchError, this, &MainController::handleFetchError);

    // 从配置管理器加载配置
    loadConfigFromManager();

    // 连接配置管理器的信号
    connect(m_configManager, &ConfigManager::configChanged, this, [this]() {
        // 配置更改时更新 UI
        qDebug() << "Configuration changed";
    });
}

MainController::~MainController()
{
}

void MainController::setOutputPath(const QString &path)
{
    if (m_outputPath != path) {
        m_outputPath = path;
        emit outputPathChanged();
    }
}

void MainController::setLibName(const QString &name)
{
    if (m_libName != name) {
        m_libName = name;
        emit libNameChanged();
    }
}

void MainController::setExportSymbol(bool enabled)
{
    if (m_exportSymbol != enabled) {
        m_exportSymbol = enabled;
        emit exportSymbolChanged();
    }
}

void MainController::setExportFootprint(bool enabled)
{
    if (m_exportFootprint != enabled) {
        m_exportFootprint = enabled;
        emit exportFootprintChanged();
    }
}

void MainController::setExportModel3D(bool enabled)
{
    if (m_exportModel3D != enabled) {
        m_exportModel3D = enabled;
        emit exportModel3DChanged();
    }
}

void MainController::setDarkMode(bool darkMode)
{
    if (m_isDarkMode != darkMode) {
        m_isDarkMode = darkMode;
        emit darkModeChanged();
        qDebug() << "Dark mode changed to:" << darkMode;
    }
}

void MainController::saveConfig()
{
    saveConfigToManager();
    m_configManager->save();
    qDebug() << "Configuration saved";
}

void MainController::resetConfig()
{
    m_configManager->resetToDefaults();
    loadConfigFromManager();
    qDebug() << "Configuration reset to defaults";
}

void MainController::addComponent(const QString &componentId)
{
    QString trimmedId = componentId.trimmed();

    if (trimmedId.isEmpty()) {
        qWarning() << "Component ID is empty";
        return;
    }

    // 验证元件ID格式
    if (!validateComponentId(trimmedId)) {
        qWarning() << "Invalid component ID format:" << trimmedId;

        // 尝试从文本中智能提取元件编号
        QStringList extractedIds = extractComponentIdFromText(trimmedId);
        if (extractedIds.isEmpty()) {
            qWarning() << "Failed to extract component ID from text:" << trimmedId;
            emit componentExported(trimmedId, false, "Invalid LCSC component ID format and failed to extract from text.");
            return;
        }

        // 如果提取到多个元件ID，只使用第一个
        qDebug() << "Extracted component ID:" << extractedIds.first();
        trimmedId = extractedIds.first();
    }

    // 检查是否已存在
    if (componentExists(trimmedId)) {
        qWarning() << "Component already exists:" << trimmedId;
        emit componentExported(trimmedId, false, "Component already in list");
        return;
    }

    // 添加到列表
    m_componentList.append(trimmedId);
    emit componentListChanged();
    emit componentCountChanged();

    qDebug() << "Component added:" << trimmedId;
}

void MainController::removeComponent(int index)
{
    if (index >= 0 && index < m_componentList.count()) {
        QString componentId = m_componentList.at(index);
        m_componentList.removeAt(index);
        emit componentListChanged();
        emit componentCountChanged();
        qDebug() << "Component removed:" << componentId;
    }
}

void MainController::clearComponentList()
{
    if (!m_componentList.isEmpty()) {
        m_componentList.clear();
        emit componentListChanged();
        emit componentCountChanged();
        qDebug() << "Component list cleared";
    }
}

void MainController::pasteFromClipboard()
{
    // 获取剪贴板内容
    QClipboard *clipboard = QGuiApplication::clipboard();
    QString clipboardText = clipboard->text();

    if (clipboardText.isEmpty()) {
        qWarning() << "Clipboard is empty";
        return;
    }

    qDebug() << "Clipboard content:" << clipboardText;

    // 使用智能提取方法识别元器件编号
    QStringList extractedIds = extractComponentIdFromText(clipboardText);

    if (extractedIds.isEmpty()) {
        qWarning() << "No valid component IDs found in clipboard";
        return;
    }

    qDebug() << "Extracted component IDs:" << extractedIds;

    // 添加到列表
    int addedCount = 0;
    for (const QString &componentId : extractedIds) {
        if (!componentExists(componentId)) {
            m_componentList.append(componentId);
            addedCount++;
        }
    }

    if (addedCount > 0) {
        emit componentListChanged();
        emit componentCountChanged();
        qDebug() << "Added" << addedCount << "components from clipboard";
    } else {
        qDebug() << "All components already exist in the list";
    }
}

void MainController::selectBomFile(const QString &filePath)
{
    if (filePath.isEmpty()) {
        qWarning() << "BOM file path is empty";
        return;
    }

    m_bomFilePath = filePath;
    emit bomFilePathChanged();

    // 解析BOM文件
    QStringList componentIds = parseBomFile(filePath);

    if (componentIds.isEmpty()) {
        m_bomResult = "No valid component IDs found in BOM file";
        emit bomResultChanged();
        return;
    }

    // 添加新元件到列表
    int addedCount = 0;
    int duplicateCount = 0;

    for (const QString &componentId : componentIds) {
        if (!componentExists(componentId)) {
            m_componentList.append(componentId);
            addedCount++;
        } else {
            duplicateCount++;
        }
    }

    if (addedCount > 0) {
        emit componentListChanged();
        emit componentCountChanged();
    }

    // 更新BOM结果
    QString result = QString("Parsed %1 valid component IDs from BOM file")
                         .arg(componentIds.count());

    if (duplicateCount > 0) {
        result += QString(", skipped %1 duplicates").arg(duplicateCount);
    }

    if (addedCount > 0) {
        result += QString(", added %1 new components").arg(addedCount);
    }

    m_bomResult = result;
    emit bomResultChanged();

    qDebug() << "BOM file imported:" << filePath << "- Added:" << addedCount << "- Duplicates:" << duplicateCount;
}

void MainController::selectOutputPath(const QString &path)
{
    if (m_outputPath != path) {
        m_outputPath = path;
        emit outputPathChanged();
        qDebug() << "Output path set to:" << path;
    }
}

void MainController::startExport()
{
    qDebug() << "Component list count:" << m_componentList.count();
    qDebug() << "Output path:" << m_outputPath << "(empty:" << m_outputPath.isEmpty() << ")";
    qDebug() << "Library name:" << m_libName << "(empty:" << m_libName.isEmpty() << ")";
    qDebug() << "Export options - Symbol:" << m_exportSymbol << "Footprint:" << m_exportFootprint << "3D Model:" << m_exportModel3D;

    if (m_componentList.isEmpty()) {
        qDebug() << "Export failed: No components to export";
        emit exportFailed("No components to export");
        return;
    }

    if (m_outputPath.isEmpty()) {
        qDebug() << "Export failed: Output path is not specified";
        emit exportFailed("Output path is not specified");
        return;
    }

    if (m_libName.isEmpty()) {
        qDebug() << "Export failed: Library name is not specified";
        emit exportFailed("Library name is not specified");
        return;
    }

    // 检查是否至少选择了一个导出选项
    qDebug() << "Checking export options...";
    qDebug() << "m_exportSymbol:" << m_exportSymbol;
    qDebug() << "m_exportFootprint:" << m_exportFootprint;
    qDebug() << "m_exportModel3D:" << m_exportModel3D;
    if (!m_exportSymbol && !m_exportFootprint && !m_exportModel3D) {
        qDebug() << "Export failed: Please select at least one export option";
        emit exportFailed("Please select at least one export option");
        return;
    }
    qDebug() << "Export options check passed";

    // 创建必要的文件夹结构（符合 Python 版本规范）
    qDebug() << "Creating output directory...";
    QDir outputDir(m_outputPath);
    if (!outputDir.exists()) {
        qDebug() << "Output directory does not exist, creating...";
        if (!outputDir.mkpath(".")) {
            qDebug() << "Failed to create output directory";
            emit exportFailed("Failed to create output directory");
            return;
        }
        qDebug() << "Output directory created successfully";
    } else {
        qDebug() << "Output directory already exists";
    }

    // 创建封装库文件夹
    if (m_exportFootprint) {
        qDebug() << "Creating footprint library directory...";
        QString footprintDirPath = QString("%1/%2.pretty").arg(m_outputPath, m_libName);
        qDebug() << "Footprint directory path:" << footprintDirPath;
        QDir footprintDir(footprintDirPath);
        if (!footprintDir.exists()) {
            qDebug() << "Footprint directory does not exist, creating...";
            if (!footprintDir.mkpath(".")) {
                qDebug() << "Failed to create footprint library directory";
                emit exportFailed("Failed to create footprint library directory");
                return;
            }
            qDebug() << "Footprint directory created successfully";
        } else {
            qDebug() << "Footprint directory already exists";
        }
    }

    // 创建 3D 模型文件夹
    if (m_exportModel3D) {
        qDebug() << "Creating 3D model directory...";
        QString model3DDirPath = QString("%1/%2.3dshapes").arg(m_outputPath, m_libName);
        qDebug() << "3D model directory path:" << model3DDirPath;
        QDir model3DDir(model3DDirPath);
        if (!model3DDir.exists()) {
            qDebug() << "3D model directory does not exist, creating...";
            if (!model3DDir.mkpath(".")) {
                qDebug() << "Failed to create 3D model directory";
                emit exportFailed("Failed to create 3D model directory");
                return;
            }
            qDebug() << "3D model directory created successfully";
        } else {
            qDebug() << "3D model directory already exists";
        }
    }

    // 开始导出
    qDebug() << "Starting export process...";
    m_isExporting = true;
    m_currentComponentIndex = 0;
    m_successCount = 0;
    m_failureCount = 0;
    m_collectedComponents.clear();
    m_isCollectingData = false;
    qDebug() << "Setting isExporting to true";
    emit isExportingChanged();
    qDebug() << "isExportingChanged signal emitted";

    qDebug() << "Updating progress...";
    updateProgress(0, m_componentList.count());
    qDebug() << "Updating status...";
    updateStatus("Starting export...");

    qDebug() << "Export started - Components:" << m_componentList.count()
             << "- Output:" << m_outputPath
             << "- Library:" << m_libName;

    qDebug() << "Calling processNextComponent()...";
    
    // 根据元件数量决定使用串行还是并行导出
    if (m_componentList.count() > 1 && m_useParallelExport) {
        // 使用并行导出 - 数据收集模式
        qDebug() << "Using parallel export (data collection mode) for" << m_componentList.count() << "components";
        m_isCollectingData = true;
        
        // 确定线程数量：16个以下使用元件数量，超过16个使用16个
        int threadCount = qMin(m_componentList.count(), 16);
        m_threadPool->setMaxThreadCount(threadCount);
        qDebug() << "Thread pool max threads:" << threadCount;
        
        // 为每个元件创建并启动数据收集任务
        for (const QString &componentId : m_componentList) {
            ComponentExportTask *task = new ComponentExportTask(
                componentId,
                m_outputPath,
                m_libName,
                m_exportSymbol,
                m_exportFootprint,
                m_exportModel3D,
                this
            );
            
            // 连接数据收集信号
            connect(task, &ComponentExportTask::dataCollected, 
                    this, &MainController::handleDataCollected);
            
            // 将任务添加到线程池
            m_threadPool->start(task);
        }
        
        qDebug() << "All data collection tasks submitted to thread pool";
    } else {
        // 使用串行导出（单个元件或禁用并行）
        qDebug() << "Using serial export";
        // 开始处理第一个元件
        processNextComponent();
    }
    qDebug() << "processNextComponent() called";
}

void MainController::processNextComponent()
{
    if (!m_isExporting || m_currentComponentIndex >= m_componentList.count()) {
        // 导出完成，一次性导出所有符号
        m_isExporting = false;
        emit isExportingChanged();
        updateProgress(100, 100);
        updateStatus("Export completed");

        // 导出符号库（一次性导出所有符号，使用追加模式）
        if (m_exportSymbol && !m_allSymbols.isEmpty()) {
            qDebug() << "=== Exporting Symbol Library ===";
            qDebug() << "Total symbols to export:" << m_allSymbols.count();
            QString symbolFilePath = QString("%1/%2.kicad_sym").arg(m_outputPath, m_libName);
            
            // 检查文件是否存在，如果存在则读取现有符号名称
            QSet<QString> existingSymbolNames;
            if (QFile::exists(symbolFilePath)) {
                qDebug() << "Symbol library already exists, reading existing symbol names...";
                QFile file(symbolFilePath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QString content = QTextStream(&file).readAll();
                    file.close();
                    
                    // 提取现有符号名称
                    QRegularExpression symbolRegex("\\(symbol\\s+\"([^\"]+)\"\\s");
                    QRegularExpressionMatchIterator it = symbolRegex.globalMatch(content);
                    while (it.hasNext()) {
                        QRegularExpressionMatch match = it.next();
                        QString symbolName = match.captured(1);
                        existingSymbolNames.insert(symbolName);
                        qDebug() << "Found existing symbol:" << symbolName;
                    }
                }
                qDebug() << "Total existing symbols:" << existingSymbolNames.count();
            }
            
            // 过滤出新符号（不在现有库中的符号）
            QList<SymbolData> newSymbols;
            for (const SymbolData &symbol : m_allSymbols) {
                if (!existingSymbolNames.contains(symbol.info().name)) {
                    newSymbols.append(symbol);
                    qDebug() << "Adding new symbol:" << symbol.info().name;
                } else {
                    qDebug() << "Symbol already exists, skipping:" << symbol.info().name;
                }
            }
            
            qDebug() << "Exporting" << newSymbols.count() << "new symbols";
            
            if (m_exporterSymbol->exportSymbolLibrary(newSymbols, m_libName, symbolFilePath,
                ExporterSymbol::KicadVersion::V6, true)) { // true = 追加模式
                qDebug() << "Symbol library exported successfully";
            } else {
                qDebug() << "Symbol library export FAILED";
            }
            // 清空符号列表
            m_allSymbols.clear();
        }

        emit exportCompleted(m_componentList.count(), m_successCount);

        qDebug() << "Export completed - Total:" << m_componentList.count()
                 << "- Success:" << m_successCount
                 << "- Failed:" << m_failureCount;
        return;
    }

    QString componentId = m_componentList.at(m_currentComponentIndex);
    updateStatus(QString("Fetching component: %1 (%2/%3)")
                     .arg(componentId)
                     .arg(m_currentComponentIndex + 1)
                     .arg(m_componentList.count()));

    // 获取组件信息
    m_easyedaApi->fetchComponentInfo(componentId);
}

void MainController::handleComponentInfoFetched(const QJsonObject &data)
{
    QString componentId = m_componentList.at(m_currentComponentIndex);
    qDebug() << "Component info fetched for:" << componentId;

    // 打印完整的 API 响应数据
    QJsonDocument doc(data);
    qDebug() << "Full API response:" << doc.toJson(QJsonDocument::Compact);

    // 检查是否有 packageDetail 字段
    if (data.contains("packageDetail")) {
        qDebug() << "packageDetail found";
    } else {
        qDebug() << "packageDetail NOT found";
    }

    // 检查 dataStr 中是否有 shape
    if (data.contains("dataStr")) {
        QJsonObject dataStr = data["dataStr"].toObject();
        if (dataStr.contains("shape")) {
            qDebug() << "shape found in dataStr";
        } else {
            qDebug() << "shape NOT found in dataStr";
        }
    }

    // 直接从已获取的数据中提取 CAD 数据
    if (data.contains("result")) {
        QJsonObject result = data["result"].toObject();
        updateStatus(QString("Processing CAD data: %1").arg(componentId));
        
        // 直接处理 CAD 数据，不需要再次请求
        handleCadDataFetched(result);
    } else {
        QString errorMsg = QString("API response missing 'result' key for component: %1").arg(componentId);
        qWarning() << errorMsg;
        m_currentComponentIndex++;
        m_failureCount++;
        updateProgress(m_currentComponentIndex, m_componentList.count());
        emit componentExported(componentId, false, errorMsg);
        processNextComponent();
    }
}

void MainController::handleCadDataFetched(const QJsonObject &data)
{
    QString componentId = m_componentList.at(m_currentComponentIndex);
    qDebug() << "CAD data fetched for:" << componentId;

#ifdef ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT
    // 保存原始 JSON 数据
    QJsonDocument jsonDoc(data);
    saveDebugData(componentId, "raw", "cad_data.json", jsonDoc.toJson(QJsonDocument::Indented));
#endif

    // 导入符号和封装数据
    QSharedPointer<SymbolData> symbolData = m_easyedaImporter->importSymbolData(data);
    QSharedPointer<FootprintData> footprintData = m_easyedaImporter->importFootprintData(data);

#ifdef ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT
    // 保存解析的符号数据
    QString symbolDebugInfo;
    QTextStream symbolStream(&symbolDebugInfo);
    symbolStream << "=== Symbol Data ===\n";
    symbolStream << "Name: " << symbolData->info().name << "\n";
    symbolStream << "Prefix: " << symbolData->info().prefix << "\n";
    symbolStream << "Package: " << symbolData->info().package << "\n";
    symbolStream << "Manufacturer: " << symbolData->info().manufacturer << "\n";
    symbolStream << "LCSC ID: " << symbolData->info().lcscId << "\n";
    symbolStream << "Pins count: " << symbolData->pins().count() << "\n";
    symbolStream << "Rectangles count: " << symbolData->rectangles().count() << "\n";
    symbolStream << "Circles count: " << symbolData->circles().count() << "\n";
    symbolStream << "Arcs count: " << symbolData->arcs().count() << "\n";
    symbolStream << "Polygons count: " << symbolData->polygons().count() << "\n";
    symbolStream << "Polylines count: " << symbolData->polylines().count() << "\n";
    symbolStream << "Paths count: " << symbolData->paths().count() << "\n";
    symbolStream << "Ellipses count: " << symbolData->ellipses().count() << "\n";
    symbolStream << "BBox: x=" << symbolData->bbox().x << ", y=" << symbolData->bbox().y << "\n";
    saveDebugData(componentId, "parsed", "symbol_data.txt", symbolDebugInfo);

    // 保存解析的封装数据
    QString footprintDebugInfo;
    QTextStream footprintStream(&footprintDebugInfo);
    footprintStream << "=== Footprint Data ===\n";
    footprintStream << "Name: " << footprintData->info().name << "\n";
    footprintStream << "Type: " << footprintData->info().type << "\n";
    footprintStream << "3D Model Name: " << footprintData->info().model3DName << "\n";
    footprintStream << "Pads count: " << footprintData->pads().count() << "\n";
    footprintStream << "Tracks count: " << footprintData->tracks().count() << "\n";
    footprintStream << "Holes count: " << footprintData->holes().count() << "\n";
    footprintStream << "Circles count: " << footprintData->circles().count() << "\n";
    footprintStream << "Rectangles count: " << footprintData->rectangles().count() << "\n";
    footprintStream << "Arcs count: " << footprintData->arcs().count() << "\n";
    footprintStream << "Texts count: " << footprintData->texts().count() << "\n";
    footprintStream << "3D Model UUID: " << footprintData->model3D().uuid() << "\n";
    footprintStream << "3D Model Name: " << footprintData->model3D().name() << "\n";
    footprintStream << "BBox: x=" << footprintData->bbox().x << ", y=" << footprintData->bbox().y << "\n";
    saveDebugData(componentId, "parsed", "footprint_data.txt", footprintDebugInfo);
#endif

    // 导出符号（改为收集所有符号，最后一次性导出）
    qDebug() << "=== Symbol Collection ===";
    qDebug() << "m_exportSymbol:" << m_exportSymbol;
    if (m_exportSymbol) {
        qDebug() << "Collecting symbol data for:" << componentId;
        m_allSymbols.append(*symbolData);
        qDebug() << "Total symbols collected:" << m_allSymbols.count();
    } else {
        qDebug() << "Symbol export is disabled (m_exportSymbol is false)";
    }

// Prepare 3D model path before exporting footprint
    QString model3DPath;
    if (m_exportModel3D && footprintData->model3D().uuid().isEmpty() == false) {
        Model3DData* model3D = new Model3DData(footprintData->model3D());

        // 清理模型名称，移除文件系统不支持的字符（符合 Python 版本规范）
        QString sanitizedName = model3D->name();
        sanitizedName.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");

        // 计算 3D 模型导出路径
        model3DPath = QString("%1/%2.3dshapes/%3.wrl")
                          .arg(m_outputPath, m_libName, sanitizedName);
    }

    // 导出封装
    QString exportedFootprintContent;
    if (m_exportFootprint) {
        QString footprintFilePath = QString("%1/%2.pretty/%3.kicad_mod")
                                        .arg(m_outputPath, m_libName, footprintData->info().name);
        if (m_exporterFootprint->exportFootprint(*footprintData, footprintFilePath, model3DPath)) {
            qDebug() << "Footprint exported for:" << componentId;

#ifdef ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT
            // 读取并保存导出的封装内容
            QFile footprintFile(footprintFilePath);
            if (footprintFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                exportedFootprintContent = QTextStream(&footprintFile).readAll();
                footprintFile.close();
                saveDebugData(componentId, "exported", "footprint_export.kicad_mod", exportedFootprintContent);
            }
#endif
        }
        if (m_exporterFootprint->exportFootprint(*footprintData, footprintFilePath, model3DPath)) {
            qDebug() << "Footprint exported for:" << componentId;
        }
    }

    // 导出 3D 模型
    if (m_exportModel3D && footprintData->model3D().uuid().isEmpty() == false) {
        updateStatus(QString("Exporting 3D model: %1").arg(componentId));
        Model3DData* model3D = new Model3DData(footprintData->model3D());

        // 保存 3D 模型信息，用于后续处理
        m_pending3DModel = model3D;
        m_pending3DModelPath = model3DPath;

        // 下载 3D 模型数据（OBJ 格式）
        qDebug() << "Downloading 3D model (OBJ) with UUID:" << model3D->uuid();
        m_easyedaApi->fetch3DModelObj(model3D->uuid());

        // 注意：不在这里调用 processNextComponent()，等待 3D 模型下载完成
        return;
    } else if (m_exportModel3D) {
        qDebug() << "No 3D model found for component:" << componentId;
    }

    // 如果没有 3D 模型需要下载，继续处理下一个组件
    m_currentComponentIndex++;
    m_successCount++;
    updateProgress(m_currentComponentIndex, m_componentList.count());
    emit componentExported(componentId, true, "Export successful");

    // 处理下一个元件
    processNextComponent();
}

void MainController::handleFetchError(const QString &errorMessage)
{
    QString componentId = m_componentList.at(m_currentComponentIndex);
    qWarning() << "Fetch error for component:" << componentId << "-" << errorMessage;

    // 清理 pending 3D model
    if (m_pending3DModel) {
        qDebug() << "Cleaning up pending 3D model for component:" << componentId;
        delete m_pending3DModel;
        m_pending3DModel = nullptr;
        m_pending3DModelPath.clear();
        m_objDownloaded = false;
    }

    m_currentComponentIndex++;
    m_failureCount++;
    updateProgress(m_currentComponentIndex, m_componentList.count());
    emit componentExported(componentId, false, errorMessage);

    // 继续处理下一个元件
    processNextComponent();
}

void MainController::cancelExport()
{
    if (m_isExporting) {
        m_isExporting = false;
        m_easyedaApi->cancelRequest();
        emit isExportingChanged();
        updateStatus("Export cancelled");
        qDebug() << "Export cancelled";
    }
}

void MainController::handleDataCollected(const QString &componentId, bool success, const QString &message)
{
    // 使用互斥锁保护共享数据
    QMutexLocker locker(m_mutex);
    
    qDebug() << "Data collected for component - ID:" << componentId << "Success:" << success << "Message:" << message;
    
    // 更新成功/失败计数
    if (success) {
        m_successCount++;
    } else {
        m_failureCount++;
    }
    
    // 通知UI
    emit componentExported(componentId, success, message);
    
    // 计算进度
    int totalCompleted = m_successCount + m_failureCount;
    int totalCount = m_componentList.count();
    int progress = static_cast<int>((static_cast<double>(totalCompleted) / totalCount) * 100);
    
    qDebug() << "Data collection progress:" << totalCompleted << "/" << totalCount << "=" << progress << "%";
    
    // 更新进度和状态
    locker.unlock();
    updateProgress(progress, totalCount);
    updateStatus(QString("Collecting data... %1/%2").arg(totalCompleted).arg(totalCount));
    locker.relock();
    
    // 检查是否所有任务都已完成
    if (totalCompleted == totalCount) {
        qDebug() << "All data collected - Total:" << totalCount 
                 << "Success:" << m_successCount 
                 << "Failed:" << m_failureCount;
        
        // 开始导出所有收集到的数据
        qDebug() << "Starting to export all collected data...";
        
        if (exportAllCollectedData()) {
            qDebug() << "All data exported successfully";
            updateProgress(100, 100);
            updateStatus("Export completed");
            
            m_isExporting = false;
            emit isExportingChanged();
            emit exportCompleted(totalCount, m_successCount);
        } else {
            qDebug() << "Failed to export all collected data";
            m_isExporting = false;
            emit isExportingChanged();
            emit exportFailed("Failed to export collected data");
        }
    }
}

bool MainController::exportAllCollectedData()
{
    qDebug() << "=== Exporting all collected data ===";
    qDebug() << "Total components to export:" << m_collectedComponents.count();
    
    if (m_collectedComponents.isEmpty()) {
        qWarning() << "No collected data to export";
        return false;
    }
    
    // 导出符号库（一次性导出所有符号到一个库文件）
    if (m_exportSymbol) {
        qDebug() << "Exporting symbol library with" << m_collectedComponents.count() << "symbols";
        
        // 收集所有符号
        QList<SymbolData> allSymbols;
        for (const auto &data : m_collectedComponents) {
            if (data.success) {
                allSymbols.append(data.symbolData);
            }
        }
        
        if (allSymbols.isEmpty()) {
            qWarning() << "No symbols to export";
        } else {
            QString symbolPath = QString("%1/%2.kicad_sym").arg(m_outputPath, m_libName);
            ExporterSymbol::KicadVersion version = ExporterSymbol::KicadVersion::V6;
            
            // 检查符号库是否已存在
            QSet<QString> existingSymbols;
            QFile existingFile(symbolPath);
            if (existingFile.exists()) {
                qDebug() << "Symbol library already exists, reading existing symbols...";
                if (existingFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&existingFile);
                    QString content = in.readAll();
                    existingFile.close();
                    
                    // 解析现有库，提取符号名称
                    QRegularExpression symbolRegex("\\(symbol\\s+\"([^\"]+)\"");
                    QRegularExpressionMatchIterator it = symbolRegex.globalMatch(content);
                    while (it.hasNext()) {
                        QRegularExpressionMatch match = it.next();
                        existingSymbols.insert(match.captured(1));
                    }
                    qDebug() << "Found" << existingSymbols.size() << "existing symbols";
                }
            }
            
            // 只导出新增的符号
            QList<SymbolData> symbolsToExport;
            for (const SymbolData &symbol : allSymbols) {
                if (!existingSymbols.contains(symbol.info().name)) {
                    symbolsToExport.append(symbol);
                } else {
                    qDebug() << "Symbol already exists, skipping:" << symbol.info().name;
                }
            }
            
            if (symbolsToExport.isEmpty()) {
                qDebug() << "No new symbols to export";
            } else {
                qDebug() << "Exporting" << symbolsToExport.size() << "new symbols";
                if (!m_exporterSymbol->exportSymbolLibrary(symbolsToExport, m_libName, symbolPath, version)) {
                    qWarning() << "Failed to export symbol library";
                    return false;
                }
                qDebug() << "Symbol library exported successfully to:" << symbolPath;
            }
        }
    }
    
    // 导出封装库（每个封装单独导出到一个文件）
    if (m_exportFootprint) {
        qDebug() << "Exporting footprint library with" << m_collectedComponents.count() << "footprints";
        
        for (const auto &data : m_collectedComponents) {
            if (!data.success) continue;
            
            QString footprintPath = QString("%1/%2.pretty/%3.kicad_mod")
                                      .arg(m_outputPath, m_libName, data.footprintData.info().name);
            
            // 检查封装文件是否已存在
            if (QFile::exists(footprintPath)) {
                qDebug() << "Footprint already exists, skipping:" << data.footprintData.info().name;
                continue;
            }
            
            QString model3DPath;
            
            if (m_exportModel3D && !data.footprintData.model3D().uuid().isEmpty()) {
                QString sanitizedName = data.footprintData.model3D().name();
                sanitizedName.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");
                // 使用绝对路径
                model3DPath = QString("%1/%2.3dshapes/%3.wrl")
                                  .arg(m_outputPath, m_libName, sanitizedName);
            }
            
            if (!m_exporterFootprint->exportFootprint(data.footprintData, footprintPath, model3DPath)) {
                qWarning() << "Failed to export footprint:" << data.footprintData.info().name;
                // 继续处理其他封装
            } else {
                qDebug() << "Footprint exported successfully:" << data.footprintData.info().name;
            }
        }
    }
    
    // 导出 3D 模型
    if (m_exportModel3D) {
        qDebug() << "Exporting 3D models";
        
        for (const auto &data : m_collectedComponents) {
            if (!data.success) continue;
            
            if (data.footprintData.model3D().uuid().isEmpty()) {
                qDebug() << "No 3D model for component:" << data.componentId;
                continue;
            }
            
            QString sanitizedName = data.footprintData.model3D().name();
            sanitizedName.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");
            QString model3DPath = QString("%1/%2.3dshapes/%3.wrl")
                                      .arg(m_outputPath, m_libName, sanitizedName);
            
            // 检查 WRL 文件是否已存在
            if (QFile::exists(model3DPath)) {
                qDebug() << "3D model WRL already exists, skipping:" << data.componentId;
            } else {
                // 导出 WRL
                            if (!data.objData.isEmpty()) {
                                qDebug() << "=== Exporting WRL for component:" << data.componentId << "===";
                                qDebug() << "OBJ data size (raw):" << data.objData.size();
                                qDebug() << "First 100 chars (raw):" << QString::fromLatin1(data.objData.left(100));
                                
                                Model3DData model3D = data.footprintData.model3D();
                                
                                // 检查是否是 gzip 压缩的数据
                                QByteArray decompressedData;
                                if (data.objData.size() >= 2 && 
                                    static_cast<unsigned char>(data.objData[0]) == 0x1F && 
                                    static_cast<unsigned char>(data.objData[1]) == 0x8B) {
                                    qDebug() << "OBJ data is gzip compressed, decompressing...";
                                    decompressedData = decompressGzip(data.objData);
                                    qDebug() << "Decompressed OBJ data size:" << decompressedData.size();
                                } else {
                                    decompressedData = data.objData;
                                }
                                
                                QString objString = QString::fromUtf8(decompressedData);
                                qDebug() << "OBJ string size:" << objString.size();
                                qDebug() << "First 200 chars (UTF-8):" << objString.left(200);
                                qDebug() << "First 10 lines:";
                                QStringList lines = objString.split('\n');
                                for (int i = 0; i < qMin(10, lines.size()); ++i) {
                                    qDebug() << "  Line" << i << ":" << lines[i].left(100);
                                }
                                
                                model3D.setRawObj(objString);
                                
                                if (!m_exporter3DModel->exportToWrl(model3D, model3DPath)) {
                                    qWarning() << "Failed to export WRL for:" << data.componentId;
                                } else {
                                    qDebug() << "3D model WRL exported successfully:" << data.componentId;
                                }
                            }            }
            
            // 导出 STEP
            if (!data.stepData.isEmpty()) {
                qDebug() << "=== Exporting STEP for component:" << data.componentId << "===";
                qDebug() << "STEP data size (raw):" << data.stepData.size();
                qDebug() << "First 100 chars (raw):" << QString::fromLatin1(data.stepData.left(100));
                
                QString stepPath = QString("%1/%2.3dshapes/%3.step")
                                      .arg(m_outputPath, m_libName, sanitizedName);
                
                // 检查 STEP 文件是否已存在
                if (QFile::exists(stepPath)) {
                    qDebug() << "3D model STEP already exists, skipping:" << data.componentId;
                } else {
                    QFile stepFile(stepPath);
                    if (stepFile.open(QIODevice::WriteOnly)) {
                        stepFile.write(data.stepData);
                        stepFile.close();
                        qDebug() << "3D model STEP exported successfully:" << data.componentId;
                    } else {
                        qWarning() << "Failed to export STEP for:" << data.componentId;
                    }
                }
            }
        }
    }
    
    qDebug() << "=== All collected data exported successfully ===";
    return true;
}

void MainController::handleComponentExportFinished(const QString &componentId, bool success, const QString &message)
{
    // 使用互斥锁保护共享数据
    QMutexLocker locker(m_mutex);
    
    qDebug() << "Component export finished - ID:" << componentId << "Success:" << success << "Message:" << message;
    
    // 更新成功/失败计数
    if (success) {
        m_successCount++;
    } else {
        m_failureCount++;
    }
    
    // 通知UI
    emit componentExported(componentId, success, message);
    
    // 计算进度
    int totalCompleted = m_successCount + m_failureCount;
    int totalCount = m_componentList.count();
    int progress = static_cast<int>((static_cast<double>(totalCompleted) / totalCount) * 100);
    
    qDebug() << "Export progress:" << totalCompleted << "/" << totalCount << "=" << progress << "%";
    
    // 更新进度和状态
    locker.unlock();
    updateProgress(progress, totalCount);
    updateStatus(QString("Exporting... %1/%2").arg(totalCompleted).arg(totalCount));
    locker.relock();
    
    // 检查是否所有任务都已完成
    if (totalCompleted == totalCount) {
        qDebug() << "All components exported - Total:" << totalCount 
                 << "Success:" << m_successCount 
                 << "Failed:" << m_failureCount;
        
        m_isExporting = false;
        emit isExportingChanged();
        
        updateProgress(100, 100);
        updateStatus("Export completed");
        
        emit exportCompleted(totalCount, m_successCount);
    }
}

bool MainController::validateComponentId(const QString &componentId) const
{
    // 验证元件ID格式：C 或 c + 至少 6 位数字
    QRegularExpression regex("^[Cc]\\d{6,}$");
    return regex.match(componentId).hasMatch();
}

QStringList MainController::extractComponentIdFromText(const QString &text) const
{
    QStringList extractedIds;
    QString trimmedText = text.trimmed();

    // 如果已经是有效的元件ID格式，直接返回
    if (validateComponentId(trimmedText)) {
        extractedIds.append(trimmedText);
        return extractedIds;
    }

    // 尝试从文本中提取元件编号
    // 支持多种格式：
    // 1. "编号：C7420375" 或 "编号:C7420375"
    // 2. "LCSC Part: C7420375" 或 "LCSC Part：C7420375"
    // 3. "C7420375"（直接提取）
    // 4. 多个元件编号，用逗号、空格或换行分隔
    // 注意：元器件编号必须在 C 或 c 后面至少有 6 个连续的数字

    // 模式1：中文冒号或英文冒号后跟C+至少6位数字
    QRegularExpression pattern1("[编号：:][：:]?\\s*([Cc]\\d{6,})");
    QRegularExpressionMatchIterator it1 = pattern1.globalMatch(trimmedText);
    while (it1.hasNext()) {
        QRegularExpressionMatch match = it1.next();
        QString extracted = match.captured(1);
        if (validateComponentId(extracted) && !extractedIds.contains(extracted)) {
            extractedIds.append(extracted);
            qDebug() << "Extracted component ID from pattern 1:" << extracted;
        }
    }

    // 模式2：LCSC Part 后跟C+至少6位数字
    QRegularExpression pattern2("[Ll][Cc][Ss][Cc]\\s*[Pp][Aa][Rr][Tt]\\s*[：:]\\s*([Cc]\\d{6,})");
    QRegularExpressionMatchIterator it2 = pattern2.globalMatch(trimmedText);
    while (it2.hasNext()) {
        QRegularExpressionMatch match = it2.next();
        QString extracted = match.captured(1);
        if (validateComponentId(extracted) && !extractedIds.contains(extracted)) {
            extractedIds.append(extracted);
            qDebug() << "Extracted component ID from pattern 2:" << extracted;
        }
    }

    // 模式3：直接查找所有 C+至少6位数字
    QRegularExpression pattern3("([Cc]\\d{6,})");
    QRegularExpressionMatchIterator it3 = pattern3.globalMatch(trimmedText);
    while (it3.hasNext()) {
        QRegularExpressionMatch match = it3.next();
        QString extracted = match.captured(1);
        if (validateComponentId(extracted) && !extractedIds.contains(extracted)) {
            extractedIds.append(extracted);
            qDebug() << "Extracted component ID from pattern 3:" << extracted;
        }
    }

    // 未找到有效的元件编号
    if (extractedIds.isEmpty()) {
        qWarning() << "Failed to extract component ID from text:" << trimmedText;
    }

    return extractedIds;
}

bool MainController::componentExists(const QString &componentId) const
{
    return m_componentList.contains(componentId);
}

QStringList MainController::parseBomFile(const QString &filePath)
{
    QStringList componentIds;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open BOM file:" << filePath;
        return componentIds;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        // 跳过空行
        if (line.isEmpty()) {
            continue;
        }

        // 验证元件ID格式
        if (validateComponentId(line)) {
            componentIds.append(line);
        }
    }

    file.close();

    qDebug() << "Parsed BOM file:" << filePath << "- Found" << componentIds.count() << "valid component IDs";

    return componentIds;
}

void MainController::updateProgress(int current, int total)
{
    if (total > 0) {
        m_progress = static_cast<int>((static_cast<double>(current) / total) * 100);
    } else {
        m_progress = 0;
    }
    emit progressChanged();
}

void MainController::updateStatus(const QString &message)
{
    if (m_status != message) {
        m_status = message;
        emit statusChanged();
    }
}

void MainController::loadConfigFromManager()
{
    // 从配置管理器加载导出路径
    m_outputPath = m_configManager->getExportPath();
    emit outputPathChanged();

    // 从配置管理器加载库名称
    m_libName = m_configManager->getLibName();
    emit libNameChanged();

    // 从配置管理器加载导出选项
    QJsonObject exportOptions = m_configManager->getExportOptions();
    
    QJsonValue symbolValue = exportOptions.value("symbol");
    m_exportSymbol = symbolValue.isUndefined() ? true : symbolValue.toBool();
    
    QJsonValue footprintValue = exportOptions.value("footprint");
    m_exportFootprint = footprintValue.isUndefined() ? true : footprintValue.toBool();
    
    QJsonValue model3dValue = exportOptions.value("model3d");
    m_exportModel3D = model3dValue.isUndefined() ? true : model3dValue.toBool();
    
    emit exportSymbolChanged();
    emit exportFootprintChanged();
    emit exportModel3DChanged();

    qDebug() << "Configuration loaded from manager";
}

void MainController::handleModel3DFetched(const QString &uuid, const QByteArray &data)
{
    qDebug() << "=== handleModel3DFetched Called ===";
    qDebug() << "UUID:" << uuid;
    qDebug() << "Data size:" << data.size();
    
    if (!m_pending3DModel) {
        qWarning() << "No pending 3D model";
        return;
    }

    if (uuid != m_pending3DModel->uuid()) {
        qWarning() << "UUID mismatch:" << uuid << "!=" << m_pending3DModel->uuid();
        return;
    }

    if (data.isEmpty()) {
        qWarning() << "3D model data is empty for UUID:" << uuid;
        // 即使数据为空，也继续处理（可能某些元件没有 3D 模型）
    }

    QString componentId = m_componentList.at(m_currentComponentIndex);

    if (!m_objDownloaded) {
        // 第一次下载：OBJ 数据
        qDebug() << "3D model OBJ data downloaded for:" << componentId;

        // 解压 gzip 数据
        QByteArray decompressedData;
        if (data.size() >= 2 && static_cast<unsigned char>(data[0]) == 0x1F && static_cast<unsigned char>(data[1]) == 0x8B) {
            qDebug() << "OBJ data is gzip compressed, decompressing...";
            decompressedData = decompressGzip(data);
            qDebug() << "Decompressed OBJ data size:" << decompressedData.size();
        } else {
            decompressedData = data;
        }

        QString objString = QString::fromUtf8(decompressedData);
        qDebug() << "OBJ string size:" << objString.size();
        qDebug() << "First 200 chars (UTF-8):" << objString.left(200);
        qDebug() << "First 10 lines:";
        QStringList lines = objString.split('\n');
        for (int i = 0; i < qMin(10, lines.size()); ++i) {
            qDebug() << "  Line" << i << ":" << lines[i].left(100);
        }

        // 保存下载的 OBJ 数据
        m_pending3DModel->setRawObj(objString);
        m_objDownloaded = true;

        // 导出为 WRL 格式
        if (m_exporter3DModel->exportToWrl(*m_pending3DModel, m_pending3DModelPath)) {
            qDebug() << "3D model WRL exported for:" << componentId;
        } else {
            qWarning() << "Failed to export 3D model WRL for:" << componentId;
        }

        // 继续下载 STEP 数据
        qDebug() << "Downloading 3D model (STEP) with UUID:" << m_pending3DModel->uuid();
        
        // 使用单次定时器确保 NetworkUtils 的状态被正确重置
        QTimer::singleShot(100, this, [this]() {
            if (m_pending3DModel) {
                m_easyedaApi->fetch3DModelStep(m_pending3DModel->uuid());
            }
        });
    } else {
        // 第二次下载：STEP 数据
        qDebug() << "3D model STEP data downloaded for:" << componentId;

        // 保存下载的 STEP 数据
        m_pending3DModel->setStep(data);

        // 导出为 STEP 格式
        QString stepPath = m_pending3DModelPath;
        stepPath.replace(".wrl", ".step");
        if (m_exporter3DModel->exportToStep(*m_pending3DModel, stepPath)) {
            qDebug() << "3D model STEP exported for:" << componentId;
        } else {
            qWarning() << "Failed to export 3D model STEP for:" << componentId;
        }

        // 释放内存
        delete m_pending3DModel;
        m_pending3DModel = nullptr;
        m_pending3DModelPath.clear();
        m_objDownloaded = false;

        // 更新进度并继续处理下一个组件
        m_currentComponentIndex++;
        updateProgress(m_currentComponentIndex, m_componentList.count());
        emit componentExported(componentId, true, "Exported successfully");
        processNextComponent();
    }
}

QByteArray MainController::decompressGzip(const QByteArray &compressedData)
{
    // 检查是否是 gzip 格式
    if (compressedData.size() < 2 || 
        static_cast<unsigned char>(compressedData[0]) != 0x1F || 
        static_cast<unsigned char>(compressedData[1]) != 0x8B) {
        qWarning() << "Data is not in gzip format";
        return compressedData;
    }

    // 初始化 zlib 解压
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = compressedData.size();
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressedData.constData()));

    // 使用 MAX_WBITS + 16 来检测 gzip 格式
    if (inflateInit2(&stream, MAX_WBITS + 16) != Z_OK) {
        qWarning() << "Failed to initialize zlib for decompression";
        return QByteArray();
    }

    QByteArray decompressed;
    char buffer[8192];

    int ret;
    do {
        stream.avail_out = sizeof(buffer);
        stream.next_out = reinterpret_cast<Bytef*>(buffer);

        ret = inflate(&stream, Z_NO_FLUSH);

        if (ret == Z_OK || ret == Z_STREAM_END) {
            int decompressedSize = sizeof(buffer) - stream.avail_out;
            decompressed.append(buffer, decompressedSize);
        } else {
            qWarning() << "Decompression error:" << ret;
            inflateEnd(&stream);
            return QByteArray();
        }
    } while (ret != Z_STREAM_END);

    inflateEnd(&stream);
    
    qDebug() << "Decompressed" << compressedData.size() << "bytes to" << decompressed.size() << "bytes";
    return decompressed;
}

void MainController::saveConfigToManager()
{
    // 保存导出路径到配置管理器
    m_configManager->setExportPath(m_outputPath);

    // 保存库名称到配置管理器
    m_configManager->setLibName(m_libName);

    // 保存导出选项到配置管理器
    QJsonObject exportOptions;
    exportOptions["symbol"] = m_exportSymbol;
    exportOptions["footprint"] = m_exportFootprint;
    exportOptions["model3d"] = m_exportModel3D;
    m_configManager->setExportOptions(exportOptions);

    qDebug() << "Configuration saved to manager";
}

// ComponentExportTask 实现

ComponentExportTask::ComponentExportTask(
    const QString &componentId,
    const QString &outputPath,
    const QString &libName,
    bool exportSymbol,
    bool exportFootprint,
    bool exportModel3D,
    MainController *controller,
    QObject *parent)
    : QObject(parent)
    , QRunnable()
    , m_componentId(componentId)
    , m_outputPath(outputPath)
    , m_libName(libName)
    , m_exportSymbol(exportSymbol)
    , m_exportFootprint(exportFootprint)
    , m_exportModel3D(exportModel3D)
    , m_controller(controller)
{
    setAutoDelete(true);
}

ComponentExportTask::~ComponentExportTask()
{
}

void ComponentExportTask::run()
{
    qDebug() << "ComponentExportTask started for:" << m_componentId;

    try {
        // 创建独立的转换引擎实例
        EasyedaApi *api = new EasyedaApi();
        EasyedaImporter *importer = new EasyedaImporter();

        // 获取组件信息
        QEventLoop loop;
        QJsonObject componentData;
        bool fetchSuccess = false;
        QString fetchError;

        // 连接信号
        QObject::connect(api, &EasyedaApi::componentInfoFetched, [&](const QJsonObject &data) {
            componentData = data;
            fetchSuccess = true;
            loop.quit();
        });

        QObject::connect(api, &EasyedaApi::fetchError, [&](const QString &error) {
            fetchError = error;
            fetchSuccess = false;
            loop.quit();
        });

        // 发起请求
        api->fetchComponentInfo(m_componentId);
        loop.exec();

        if (!fetchSuccess) {
            emit dataCollected(m_componentId, false, fetchError);
            delete api;
            delete importer;
            return;
        }

        // 检查返回的数据结构
        qDebug() << "Component data keys:" << componentData.keys();
        
        // API返回的数据结构：{ "success": true, "code": 0, "result": {...} }
        // 需要提取 result 字段
        QJsonObject resultData;
        if (componentData.contains("result")) {
            resultData = componentData["result"].toObject();
            qDebug() << "Result data keys:" << resultData.keys();
        } else {
            emit dataCollected(m_componentId, false, "API response missing 'result' field");
            delete api;
            delete importer;
            return;
        }

        // 导入数据
        QSharedPointer<SymbolData> symbolDataPtr = importer->importSymbolData(resultData);
        QSharedPointer<FootprintData> footprintDataPtr = importer->importFootprintData(resultData);
        
        if (!symbolDataPtr || !footprintDataPtr) {
            emit dataCollected(m_componentId, false, "Failed to import component data");
            delete api;
            delete importer;
            return;
        }
        
        SymbolData symbolData = *symbolDataPtr;
        FootprintData footprintData = *footprintDataPtr;
        
        qDebug() << "Imported symbol - Name:" << symbolData.info().name;
        qDebug() << "Imported footprint - Name:" << footprintData.info().name;

        // 收集 3D 模型数据
        QByteArray objData;
        QByteArray stepData;
        
        if (m_exportModel3D && !footprintData.model3D().uuid().isEmpty()) {
            qDebug() << "=== Collecting 3D model data for:" << m_componentId << "===";
            qDebug() << "3D model UUID:" << footprintData.model3D().uuid();
            
            QString modelUuid = footprintData.model3D().uuid();

            // 为 OBJ 数据下载创建独立的 EasyedaApi 实例
            EasyedaApi *objApi = new EasyedaApi();
            QEventLoop objLoop;
            bool objSuccess = false;
            QString objError;

            QObject::connect(objApi, &EasyedaApi::model3DFetched, [&](const QString &uuid, const QByteArray &data) {
                qDebug() << "OBJ model3DFetched signal - Expected UUID:" << modelUuid << "Received UUID:" << uuid;
                if (uuid == modelUuid) {
                    qDebug() << "UUID match! Storing OBJ data, size:" << data.size();
                    qDebug() << "First 100 chars (raw):" << QString::fromLatin1(data.left(100));
                    objData = data;
                    objSuccess = true;
                    objLoop.quit();
                }
            });

            QObject::connect(objApi, &EasyedaApi::fetchError, [&](const QString &error) {
                qDebug() << "OBJ fetchError signal - Error:" << error;
                objError = error;
                objSuccess = false;
                objLoop.quit();
            });

            qDebug() << "Fetching OBJ data for UUID:" << modelUuid;
            objApi->fetch3DModelObj(modelUuid);
            objLoop.exec();

            // 删除 OBJ API 实例，断开所有连接
            objApi->deleteLater();

            if (!objSuccess) {
                qWarning() << "Failed to download OBJ data for:" << m_componentId << "-" << objError;
            } else {
                qDebug() << "OBJ data collected successfully, size:" << objData.size();
            }

            // 为 STEP 数据下载创建完全独立的 EasyedaApi 实例
            EasyedaApi *stepApi = new EasyedaApi();
            QEventLoop stepLoop;
            bool stepSuccess = false;
            QString stepError;

            QObject::connect(stepApi, &EasyedaApi::model3DFetched, [&](const QString &uuid, const QByteArray &data) {
                qDebug() << "STEP model3DFetched signal - Expected UUID:" << modelUuid << "Received UUID:" << uuid;
                if (uuid == modelUuid) {
                    qDebug() << "UUID match! Storing STEP data, size:" << data.size();
                    qDebug() << "First 100 chars (raw):" << QString::fromLatin1(data.left(100));
                    stepData = data;
                    stepSuccess = true;
                    stepLoop.quit();
                }
            });

            QObject::connect(stepApi, &EasyedaApi::fetchError, [&](const QString &error) {
                qDebug() << "STEP fetchError signal - Error:" << error;
                stepError = error;
                stepSuccess = false;
                stepLoop.quit();
            });

            qDebug() << "Fetching STEP data for UUID:" << modelUuid;
            stepApi->fetch3DModelStep(modelUuid);
            stepLoop.exec();

            // 删除 STEP API 实例，断开所有连接
            stepApi->deleteLater();

            if (!stepSuccess) {
                qWarning() << "Failed to download STEP data for:" << m_componentId << "-" << stepError;
            } else {
                qDebug() << "STEP data collected successfully, size:" << stepData.size();
            }
            
            qDebug() << "=== 3D model data collection completed for:" << m_componentId << "===";
        }

        // 将收集到的数据存储到 MainController
        {
            QMutexLocker locker(m_controller->m_mutex);
            MainController::CollectedComponentData collectedData;
            collectedData.componentId = m_componentId;
            collectedData.symbolData = symbolData;
            collectedData.footprintData = footprintData;
            collectedData.objData = objData;
            collectedData.stepData = stepData;
            collectedData.success = true;
            collectedData.errorMessage = "";
            
            m_controller->m_collectedComponents.append(collectedData);
            qDebug() << "Data collected and stored for:" << m_componentId;
        }

        // 清理资源
        delete api;
        delete importer;

        qDebug() << "ComponentExportTask data collection completed for:" << m_componentId;
        emit dataCollected(m_componentId, true, "Data collected successfully");

    } catch (const std::exception &e) {
        qWarning() << "Exception in ComponentExportTask for" << m_componentId << ":" << e.what();
        emit dataCollected(m_componentId, false, QString("Exception: %1").arg(e.what()));
    } catch (...) {
        qWarning() << "Unknown exception in ComponentExportTask for:" << m_componentId;
        emit dataCollected(m_componentId, false, "Unknown exception occurred");
    }
}

#ifdef ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT
QString MainController::getDebugDataDir(const QString &componentId) const
{
    QString debugDir = QString("%1/debug_data/%2").arg(m_outputPath, componentId);
    QDir dir;
    if (!dir.exists(debugDir)) {
        dir.mkpath(debugDir);
    }
    return debugDir;
}

void MainController::saveDebugData(const QString &componentId, const QString &dataType, const QString &filename, const QString &content)
{
    QString debugDir = getDebugDataDir(componentId);
    QString filePath = QString("%1/%2_%3").arg(debugDir, dataType, filename);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << content;
        file.close();
        qDebug() << "Debug data saved to:" << filePath;
    } else {
        qWarning() << "Failed to save debug data to:" << filePath;
    }
}
#endif

} // namespace EasyKiConverter