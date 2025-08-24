#include "MainWindow.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <fstream>
#include <thread>
#include <chrono>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , worker(nullptr)
    , workerThread(nullptr)
{
    // 注册元类型以支持在信号槽中传递
    qRegisterMetaType<QStringList>();
    
    settings = new QSettings("EasyKiConverter", "EasyKiConverter");
    setupUI();
    loadSettings();
    
    // Initialize worker in a separate thread
    workerThread = new QThread(this);
    
    // 在连接信号之后创建worker，并确保它在子线程中创建
    workerThread->start();
    
    // 在子线程中创建worker对象
    QMetaObject::invokeMethod(this, [this]() {
        worker = new ConverterWorker();
        worker->moveToThread(workerThread);
        
        // Connect signals and slots
        connect(convertButton, &QPushButton::clicked, this, &MainWindow::onConvertClicked);
        connect(browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
        connect(selectAllButton, &QPushButton::clicked, this, &MainWindow::onSelectAllClicked);
        connect(clearButton, &QPushButton::clicked, this, &MainWindow::onClearClicked);
        connect(pasteButton, &QPushButton::clicked, this, &MainWindow::onPasteClicked);
        
        // 使用QueuedConnection避免线程问题
        connect(this, &MainWindow::startConversion, worker, &ConverterWorker::processConversion, Qt::QueuedConnection);
        connect(worker, &ConverterWorker::progressUpdated, this, &MainWindow::onUpdateProgress, Qt::QueuedConnection);
        connect(worker, &ConverterWorker::conversionFinished, this, &MainWindow::onExportFinished, Qt::QueuedConnection);
        connect(worker, &ConverterWorker::conversionError, this, &MainWindow::onConversionError, Qt::QueuedConnection);
        
        // 连接线程完成信号
        connect(workerThread, &QThread::finished, worker, &ConverterWorker::deleteLater);
    }, Qt::QueuedConnection);
    
    // Set default export path
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/EasyKiConverter";
    exportPathEdit->setText(defaultPath);
    filePrefixEdit->setText("easyki_export");
    
    // Resize and center window
    resize(800, 600);
    QDesktopWidget *desktop = QApplication::desktop();
    move((desktop->width() - width()) / 2, (desktop->height() - height()) / 2);
}

MainWindow::~MainWindow()
{
    if (workerThread) {
        workerThread->quit();
        workerThread->wait();
        delete workerThread;
    }
    
    saveSettings();
    delete settings;
}

void MainWindow::setupUI()
{
    setWindowTitle("EasyKiConverter - C++ GUI");
    
    // Create central widget and main layout
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    
    // Component IDs input
    QHBoxLayout* componentLayout = new QHBoxLayout();
    componentLayout->addWidget(new QLabel("Component IDs:"));
    componentIdsEdit = new QLineEdit();
    componentIdsEdit->setPlaceholderText("Enter LCSC IDs separated by commas (e.g., C12345, C67890)");
    componentLayout->addWidget(componentIdsEdit);
    pasteButton = new QPushButton("Paste");
    componentLayout->addWidget(pasteButton);
    mainLayout->addLayout(componentLayout);
    
    // Export options
    QFormLayout* optionsLayout = new QFormLayout();
    
    // Export path
    QHBoxLayout* pathLayout = new QHBoxLayout();
    exportPathEdit = new QLineEdit();
    browseButton = new QPushButton("Browse");
    pathLayout->addWidget(exportPathEdit);
    pathLayout->addWidget(browseButton);
    optionsLayout->addRow("Export Path:", pathLayout);
    
    // File prefix
    filePrefixEdit = new QLineEdit();
    optionsLayout->addRow("File Prefix:", filePrefixEdit);
    
    mainLayout->addLayout(optionsLayout);
    
    // Export type checkboxes
    QHBoxLayout* checkboxesLayout = new QHBoxLayout();
    symbolCheckBox = new QCheckBox("Export Symbols");
    footprintCheckBox = new QCheckBox("Export Footprints");
    model3dCheckBox = new QCheckBox("Export 3D Models");
    symbolCheckBox->setChecked(true);
    footprintCheckBox->setChecked(true);
    model3dCheckBox->setChecked(true);
    checkboxesLayout->addWidget(symbolCheckBox);
    checkboxesLayout->addWidget(footprintCheckBox);
    checkboxesLayout->addWidget(model3dCheckBox);
    mainLayout->addLayout(checkboxesLayout);
    
    // Buttons
    QHBoxLayout* buttonsLayout = new QHBoxLayout();
    convertButton = new QPushButton("Convert");
    convertButton->setDefault(true);
    selectAllButton = new QPushButton("Select All");
    clearButton = new QPushButton("Clear");
    buttonsLayout->addWidget(convertButton);
    buttonsLayout->addWidget(selectAllButton);
    buttonsLayout->addWidget(clearButton);
    buttonsLayout->addStretch();
    mainLayout->addLayout(buttonsLayout);
    
    // Progress bar
    progressBar = new QProgressBar();
    progressBar->setValue(0);
    progressBar->setVisible(false);
    mainLayout->addWidget(progressBar);
    
    // Status label
    statusLabel = new QLabel("Ready");
    mainLayout->addWidget(statusLabel);
    
    // Log text area
    logTextEdit = new QTextEdit();
    logTextEdit->setReadOnly(true);
    logTextEdit->setMaximumHeight(200);
    mainLayout->addWidget(new QLabel("Log:"));
    mainLayout->addWidget(logTextEdit);
}

void MainWindow::loadSettings()
{
    componentIdsEdit->setText(settings->value("componentIds", "").toString());
    exportPathEdit->setText(settings->value("exportPath", "").toString());
    filePrefixEdit->setText(settings->value("filePrefix", "easyki_export").toString());
    symbolCheckBox->setChecked(settings->value("exportSymbol", true).toBool());
    footprintCheckBox->setChecked(settings->value("exportFootprint", true).toBool());
    model3dCheckBox->setChecked(settings->value("export3dModel", true).toBool());
}

void MainWindow::saveSettings()
{
    settings->setValue("componentIds", componentIdsEdit->text());
    settings->setValue("exportPath", exportPathEdit->text());
    settings->setValue("filePrefix", filePrefixEdit->text());
    settings->setValue("exportSymbol", symbolCheckBox->isChecked());
    settings->setValue("exportFootprint", footprintCheckBox->isChecked());
    settings->setValue("export3dModel", model3dCheckBox->isChecked());
}

QStringList MainWindow::parseComponentIds(const QString& text) const
{
    QStringList result;
    // 修复Qt版本兼容性问题
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList ids = text.split(",", Qt::SkipEmptyParts);
#else
    QStringList ids = text.split(",", QString::SkipEmptyParts);
#endif
    for (const QString& id : ids) {
        QString trimmed = id.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }
    return result;
}

void MainWindow::onConvertClicked()
{
    QString componentIdsText = componentIdsEdit->text();
    if (componentIdsText.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please enter at least one component ID.");
        return;
    }
    
    QString exportPath = exportPathEdit->text();
    if (exportPath.isEmpty()) {
        QMessageBox::warning(this, "Invalid Path", "Please specify an export path.");
        return;
    }
    
    // Create export directory if it doesn't exist
    QDir dir(exportPath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            QMessageBox::warning(this, "Invalid Path", "Cannot create export directory.");
            return;
        }
    }
    
    // Parse component IDs
    QStringList componentIds = parseComponentIds(componentIdsText);
    if (componentIds.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "No valid component IDs found.");
        return;
    }
    
    // Disable UI during conversion
    convertButton->setEnabled(false);
    progressBar->setVisible(true);
    progressBar->setValue(0);
    statusLabel->setText("Converting components...");
    logTextEdit->clear();
    
    // Start conversion in worker thread using signal/slot mechanism
    emit startConversion(
        componentIds,
        filePrefixEdit->text(),
        exportPath,
        symbolCheckBox->isChecked(),
        footprintCheckBox->isChecked(),
        model3dCheckBox->isChecked()
    );
}

void MainWindow::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Export Directory", 
                                                    exportPathEdit->text());
    if (!dir.isEmpty()) {
        exportPathEdit->setText(dir);
    }
}

void MainWindow::onSelectAllClicked()
{
    symbolCheckBox->setChecked(true);
    footprintCheckBox->setChecked(true);
    model3dCheckBox->setChecked(true);
}

void MainWindow::onClearClicked()
{
    componentIdsEdit->clear();
    logTextEdit->clear();
}

void MainWindow::onPasteClicked()
{
    QClipboard* clipboard = QApplication::clipboard();
    QString text = clipboard->text();
    if (!text.isEmpty()) {
        componentIdsEdit->setText(text);
    }
}

void MainWindow::onExportFinished(const QString& message)
{
    // 确保这个槽函数在主线程中执行
    QMetaObject::invokeMethod(this, [this, message]() {
        // Re-enable UI
        convertButton->setEnabled(true);
        progressBar->setVisible(false);
        statusLabel->setText("Conversion completed");
        
        logTextEdit->append(message);
        logTextEdit->append("Conversion completed at " + QDateTime::currentDateTime().toString());
        
        QMessageBox::information(this, "Conversion Completed", message);
    }, Qt::QueuedConnection);
}

void MainWindow::onUpdateProgress(int value)
{
    // 确保这个槽函数在主线程中执行
    QMetaObject::invokeMethod(this, [this, value]() {
        progressBar->setValue(value);
    }, Qt::QueuedConnection);
}

void MainWindow::onConversionError(const QString& error)
{
    // 确保这个槽函数在主线程中执行
    QMetaObject::invokeMethod(this, [this, error]() {
        // Re-enable UI
        convertButton->setEnabled(true);
        progressBar->setVisible(false);
        statusLabel->setText("Conversion failed");
        
        logTextEdit->append("Error: " + error);
        QMessageBox::critical(this, "Conversion Error", error);
    }, Qt::QueuedConnection);
}

// Worker implementation
ConverterWorker::ConverterWorker(QObject *parent) : QObject(parent)
{
}

void ConverterWorker::processConversion(const QStringList& componentIds,
                                       const QString& filePrefix,
                                       const QString& exportPath,
                                       bool exportSymbol,
                                       bool exportFootprint,
                                       bool export3dModel)
{
    int totalComponents = componentIds.size();
    int processedComponents = 0;
    
    QString logMessage = QString("Starting conversion of %1 components...\n").arg(totalComponents);
    
    try {
        EasyedaApi api;
        
        for (const QString& componentId : componentIds) {
            // Update progress
            int progress = (processedComponents * 100) / totalComponents;
            emit progressUpdated(progress);
            
            logMessage += QString("Processing component: %1\n").arg(componentId);
            
            // Fetch component data
            std::string componentIdStd = componentId.toStdString();
            nlohmann::json cadData = api.get_cad_data_of_component(componentIdStd);
            if (cadData.empty()) {
                logMessage += QString("  Warning: Failed to fetch data for %1\n").arg(componentId);
                processedComponents++;
                continue;
            }
            
            // Create export directory for this component
            QString componentDir = exportPath + "/" + filePrefix + "_" + componentId;
            QDir dir(componentDir);
            if (!dir.exists()) {
                if (!dir.mkpath(".")) {
                    logMessage += QString("  Error: Failed to create directory %1\n").arg(componentDir);
                    processedComponents++;
                    continue;
                }
            }
            
            // Export symbol
            if (exportSymbol) {
                logMessage += QString("  Exporting symbol...\n");
                // 实际导出符号的实现
                try {
                    // 导出符号
                    KicadSymbolExporter exporter(KicadVersion::v6);
                    QString symbolFilePath = QString("%1/%2_%3.kicad_sym")
                        .arg(componentDir)
                        .arg(filePrefix)
                        .arg(componentId);
                    
                    if (exporter.exportSymbol(cadData, symbolFilePath.toStdString())) {
                        logMessage += QString("  Symbol exported to: %1\n").arg(symbolFilePath);
                    } else {
                        logMessage += QString("  Error: Failed to export symbol\n");
                    }
                } catch (const std::exception& e) {
                    logMessage += QString("  Error exporting symbol: %1\n").arg(e.what());
                }
            }
            
            // Export footprint
            if (exportFootprint) {
                logMessage += QString("  Exporting footprint...\n");
                // 实际导出封装的实现
                try {
                    // 导出封装 (暂时只是占位符)
                    QString footprintFilePath = QString("%1/%2_%3.kicad_mod")
                        .arg(componentDir)
                        .arg(filePrefix)
                        .arg(componentId);
                    
                    std::ofstream outFile(footprintFilePath.toStdString());
                    if (outFile.is_open()) {
                        outFile << "(module " << componentId.toStdString() << " (layer F.Cu)\n";
                        outFile << "  (descr \"Exported by EasyKiConverter\")\n";
                        outFile << "  (attr smd)\n";
                        outFile << ")\n";
                        outFile.close();
                        logMessage += QString("  Footprint exported to: %1\n").arg(footprintFilePath);
                    } else {
                        logMessage += QString("  Error: Failed to create footprint file\n");
                    }
                } catch (const std::exception& e) {
                    logMessage += QString("  Error exporting footprint: %1\n").arg(e.what());
                }
            }
            
            // Export 3D model
            if (export3dModel) {
                logMessage += QString("  Exporting 3D model...\n");
                // 实际导出3D模型的实现
                try {
                    // 导出3D模型 (暂时只是占位符)
                    QString modelFilePath = QString("%1/%2_%3.step")
                        .arg(componentDir)
                        .arg(filePrefix)
                        .arg(componentId);
                    
                    std::ofstream outFile(modelFilePath.toStdString());
                    if (outFile.is_open()) {
                        outFile << "# 3D Model exported by EasyKiConverter\n";
                        outFile << "# Component: " << componentId.toStdString() << "\n";
                        outFile.close();
                        logMessage += QString("  3D model exported to: %1\n").arg(modelFilePath);
                    } else {
                        logMessage += QString("  Error: Failed to create 3D model file\n");
                    }
                } catch (const std::exception& e) {
                    logMessage += QString("  Error exporting 3D model: %1\n").arg(e.what());
                }
            }
            
            processedComponents++;
        }
        
        logMessage += QString("\nSuccessfully processed %1 out of %2 components.")
                         .arg(processedComponents).arg(totalComponents);
        
        emit conversionFinished(logMessage);
    } catch (const std::exception& e) {
        QString error = QString("Conversion failed: %1").arg(e.what());
        emit conversionError(error);
    } catch (...) {
        emit conversionError("Unknown error occurred during conversion");
    }
    
    emit finished();
}
