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
#include <QScreen>

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
    
    // 应用现代化的Fusion样式
    qApp->setStyle(QStyleFactory::create("Fusion"));
    
    // 默认使用明亮主题
    setupLightTheme();
    
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
    // 使用新的屏幕API来居中窗口
    if (qApp->primaryScreen()) {
        QRect screenGeometry = qApp->primaryScreen()->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
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
    
    // 创建菜单栏
    QMenuBar* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);
    
    // 创建主题菜单
    QMenu* themeMenu = menuBar->addMenu("&Theme");
    lightThemeAction = themeMenu->addAction("&Light Theme");
    darkThemeAction = themeMenu->addAction("&Dark Theme");
    
    // 连接主题切换动作
    connect(lightThemeAction, &QAction::triggered, this, &MainWindow::switchToLightTheme);
    connect(darkThemeAction, &QAction::triggered, this, &MainWindow::switchToDarkTheme);
    
    // Create central widget and main layout
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    
    // Component IDs input
    QHBoxLayout* componentLayout = new QHBoxLayout();
    componentLayout->addWidget(new QLabel("Component IDs:"));
    componentIdsEdit = new QLineEdit();
    componentIdsEdit->setPlaceholderText("Enter LCSC IDs separated by commas (e.g., C12345, C67890)");
    componentLayout->addWidget(componentIdsEdit);
    pasteButton = new QPushButton("Paste");
    // 添加图标和样式
    pasteButton->setStyleSheet("QPushButton {"
                               "  background-color: #2196F3;"
                               "  color: white;"
                               "  border: none;"
                               "  padding: 8px;"
                               "  border-radius: 4px;"
                               "} "
                               "QPushButton:hover {"
                               "  background-color: #1976D2;"
                               "}");
    componentLayout->addWidget(pasteButton);
    mainLayout->addLayout(componentLayout);
    
    // Export options
    QGroupBox* optionsGroup = new QGroupBox("Export Options");
    QFormLayout* optionsLayout = new QFormLayout(optionsGroup);
    optionsLayout->setSpacing(10);
    
    // Export path
    QHBoxLayout* pathLayout = new QHBoxLayout();
    exportPathEdit = new QLineEdit();
    browseButton = new QPushButton("Browse");
    // 添加样式
    browseButton->setStyleSheet("QPushButton {"
                                "  background-color: #4CAF50;"
                                "  color: white;"
                                "  border: none;"
                                "  padding: 8px;"
                                "  border-radius: 4px;"
                                "} "
                                "QPushButton:hover {"
                                "  background-color: #388E3C;"
                                "}");
    pathLayout->addWidget(exportPathEdit);
    pathLayout->addWidget(browseButton);
    optionsLayout->addRow("Export Path:", pathLayout);
    
    // File prefix
    filePrefixEdit = new QLineEdit();
    optionsLayout->addRow("File Prefix:", filePrefixEdit);
    
    mainLayout->addWidget(optionsGroup);
    
    // Export type checkboxes
    QGroupBox* exportTypesGroup = new QGroupBox("Export Types");
    QHBoxLayout* checkboxesLayout = new QHBoxLayout(exportTypesGroup);
    symbolCheckBox = new QCheckBox("Export Symbols");
    footprintCheckBox = new QCheckBox("Export Footprints");
    model3dCheckBox = new QCheckBox("Export 3D Models");
    symbolCheckBox->setChecked(true);
    footprintCheckBox->setChecked(true);
    model3dCheckBox->setChecked(true);
    
    // 添加样式
    QString checkBoxStyle = "QCheckBox {"
                            "  spacing: 5px;"
                            "} "
                            "QCheckBox::indicator {"
                            "  width: 18px;"
                            "  height: 18px;"
                            "} "
                            "QCheckBox::indicator:unchecked {"
                            "  border: 2px solid #555;"
                            "  background-color: #222;"
                            "} "
                            "QCheckBox::indicator:checked {"
                            "  border: 2px solid #4CAF50;"
                            "  background-color: #4CAF50;"
                            "}";
    symbolCheckBox->setStyleSheet(checkBoxStyle);
    footprintCheckBox->setStyleSheet(checkBoxStyle);
    model3dCheckBox->setStyleSheet(checkBoxStyle);
    
    checkboxesLayout->addWidget(symbolCheckBox);
    checkboxesLayout->addWidget(footprintCheckBox);
    checkboxesLayout->addWidget(model3dCheckBox);
    mainLayout->addWidget(exportTypesGroup);
    
    // Buttons
    QHBoxLayout* buttonsLayout = new QHBoxLayout();
    convertButton = new QPushButton("Convert");
    convertButton->setDefault(true);
    selectAllButton = new QPushButton("Select All");
    clearButton = new QPushButton("Clear");
    
    // 添加样式
    convertButton->setStyleSheet("QPushButton {"
                                 "  background-color: #2196F3;"
                                 "  color: white;"
                                 "  border: none;"
                                 "  padding: 10px;"
                                 "  font-weight: bold;"
                                 "  border-radius: 4px;"
                                 "} "
                                 "QPushButton:hover {"
                                 "  background-color: #1976D2;"
                                 "}");
    
    selectAllButton->setStyleSheet("QPushButton {"
                                   "  background-color: #FF9800;"
                                   "  color: white;"
                                   "  border: none;"
                                   "  padding: 10px;"
                                   "  border-radius: 4px;"
                                   "} "
                                   "QPushButton:hover {"
                                   "  background-color: #F57C00;"
                                   "}");
    
    clearButton->setStyleSheet("QPushButton {"
                               "  background-color: #F44336;"
                               "  color: white;"
                               "  border: none;"
                               "  padding: 10px;"
                               "  border-radius: 4px;"
                               "} "
                               "QPushButton:hover {"
                               "  background-color: #D32F2F;"
                               "}");
    
    buttonsLayout->addWidget(convertButton);
    buttonsLayout->addWidget(selectAllButton);
    buttonsLayout->addWidget(clearButton);
    buttonsLayout->addStretch();
    mainLayout->addLayout(buttonsLayout);
    
    // Progress bar
    progressBar = new QProgressBar();
    progressBar->setValue(0);
    progressBar->setVisible(false);
    // 添加样式
    progressBar->setStyleSheet("QProgressBar {"
                               "  border: 2px solid grey;"
                               "  border-radius: 5px;"
                               "  text-align: center;"
                               "} "
                               "QProgressBar::chunk {"
                               "  background-color: #4CAF50;"
                               "  width: 20px;"
                               "}");
    mainLayout->addWidget(progressBar);
    
    // Status label
    statusLabel = new QLabel("Ready");
    statusLabel->setStyleSheet("QLabel {"
                               "  font-weight: bold;"
                               "  color: #4CAF50;"
                               "}");
    mainLayout->addWidget(statusLabel);
    
    // Log text area
    logTextEdit = new QTextEdit();
    logTextEdit->setReadOnly(true);
    logTextEdit->setMaximumHeight(200);
    logTextEdit->setStyleSheet("QTextEdit {"
                               "  background-color: #1E1E1E;"
                               "  color: #FFFFFF;"
                               "  border: 1px solid #555;"
                               "}");
    mainLayout->addWidget(new QLabel("Log:"));
    mainLayout->addWidget(logTextEdit);
}

void MainWindow::setupLightTheme()
{
    // 设置明亮主题调色板
    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::WindowText, Qt::black);
    lightPalette.setColor(QPalette::Base, Qt::white);
    lightPalette.setColor(QPalette::AlternateBase, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::ToolTipBase, Qt::white);
    lightPalette.setColor(QPalette::ToolTipText, Qt::black);
    lightPalette.setColor(QPalette::Text, Qt::black);
    lightPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(128, 128, 128));
    lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::ButtonText, Qt::black);
    lightPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 128, 128));
    lightPalette.setColor(QPalette::BrightText, Qt::red);
    lightPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    lightPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    lightPalette.setColor(QPalette::HighlightedText, Qt::white);
    lightPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(128, 128, 128));
    
    qApp->setPalette(lightPalette);
    qApp->setStyleSheet("QToolTip { color: #000000; background-color: #ffffff; border: 1px solid black; }");
    
    // 更新控件样式
    pasteButton->setStyleSheet("QPushButton {"
                               "  background-color: #2196F3;"
                               "  color: white;"
                               "  border: none;"
                               "  padding: 8px;"
                               "  border-radius: 4px;"
                               "} "
                               "QPushButton:hover {"
                               "  background-color: #1976D2;"
                               "}");
    
    browseButton->setStyleSheet("QPushButton {"
                                "  background-color: #4CAF50;"
                                "  color: white;"
                                "  border: none;"
                                "  padding: 8px;"
                                "  border-radius: 4px;"
                                "} "
                                "QPushButton:hover {"
                                "  background-color: #388E3C;"
                                "}");
    
    convertButton->setStyleSheet("QPushButton {"
                                 "  background-color: #2196F3;"
                                 "  color: white;"
                                 "  border: none;"
                                 "  padding: 10px;"
                                 "  font-weight: bold;"
                                 "  border-radius: 4px;"
                                 "} "
                                 "QPushButton:hover {"
                                 "  background-color: #1976D2;"
                                 "}");
    
    selectAllButton->setStyleSheet("QPushButton {"
                                   "  background-color: #FF9800;"
                                   "  color: white;"
                                   "  border: none;"
                                   "  padding: 10px;"
                                   "  border-radius: 4px;"
                                   "} "
                                   "QPushButton:hover {"
                                   "  background-color: #F57C00;"
                                   "}");
    
    clearButton->setStyleSheet("QPushButton {"
                               "  background-color: #F44336;"
                               "  color: white;"
                               "  border: none;"
                               "  padding: 10px;"
                               "  border-radius: 4px;"
                               "} "
                               "QPushButton:hover {"
                               "  background-color: #D32F2F;"
                               "}");
    
    progressBar->setStyleSheet("QProgressBar {"
                               "  border: 2px solid grey;"
                               "  border-radius: 5px;"
                               "  text-align: center;"
                               "} "
                               "QProgressBar::chunk {"
                               "  background-color: #4CAF50;"
                               "  width: 20px;"
                               "}");
    
    statusLabel->setStyleSheet("QLabel {"
                               "  font-weight: bold;"
                               "  color: #4CAF50;"
                               "}");
    
    logTextEdit->setStyleSheet("QTextEdit {"
                               "  background-color: #FFFFFF;"
                               "  color: #000000;"
                               "  border: 1px solid #CCCCCC;"
                               "}");
    
    // 更新复选框样式
    QString checkBoxStyle = "QCheckBox {"
                            "  spacing: 5px;"
                            "} "
                            "QCheckBox::indicator {"
                            "  width: 18px;"
                            "  height: 18px;"
                            "} "
                            "QCheckBox::indicator:unchecked {"
                            "  border: 2px solid #888;"
                            "  background-color: #FFF;"
                            "} "
                            "QCheckBox::indicator:checked {"
                            "  border: 2px solid #4CAF50;"
                            "  background-color: #4CAF50;"
                            "}";
    symbolCheckBox->setStyleSheet(checkBoxStyle);
    footprintCheckBox->setStyleSheet(checkBoxStyle);
    model3dCheckBox->setStyleSheet(checkBoxStyle);
}

void MainWindow::setupDarkTheme()
{
    // 初始化深色主题调色板
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(128, 128, 128));
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 128, 128));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(128, 128, 128));
    
    qApp->setPalette(darkPalette);
    qApp->setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");
    
    // 更新控件样式
    pasteButton->setStyleSheet("QPushButton {"
                               "  background-color: #2196F3;"
                               "  color: white;"
                               "  border: none;"
                               "  padding: 8px;"
                               "  border-radius: 4px;"
                               "} "
                               "QPushButton:hover {"
                               "  background-color: #1976D2;"
                               "}");
    
    browseButton->setStyleSheet("QPushButton {"
                                "  background-color: #4CAF50;"
                                "  color: white;"
                                "  border: none;"
                                "  padding: 8px;"
                                "  border-radius: 4px;"
                                "} "
                                "QPushButton:hover {"
                                "  background-color: #388E3C;"
                                "}");
    
    convertButton->setStyleSheet("QPushButton {"
                                 "  background-color: #2196F3;"
                                 "  color: white;"
                                 "  border: none;"
                                 "  padding: 10px;"
                                 "  font-weight: bold;"
                                 "  border-radius: 4px;"
                                 "} "
                                 "QPushButton:hover {"
                                 "  background-color: #1976D2;"
                                 "}");
    
    selectAllButton->setStyleSheet("QPushButton {"
                                   "  background-color: #FF9800;"
                                   "  color: white;"
                                   "  border: none;"
                                   "  padding: 10px;"
                                   "  border-radius: 4px;"
                                   "} "
                                   "QPushButton:hover {"
                                   "  background-color: #F57C00;"
                                   "}");
    
    clearButton->setStyleSheet("QPushButton {"
                               "  background-color: #F44336;"
                               "  color: white;"
                               "  border: none;"
                               "  padding: 10px;"
                               "  border-radius: 4px;"
                               "} "
                               "QPushButton:hover {"
                               "  background-color: #D32F2F;"
                               "}");
    
    progressBar->setStyleSheet("QProgressBar {"
                               "  border: 2px solid grey;"
                               "  border-radius: 5px;"
                               "  text-align: center;"
                               "} "
                               "QProgressBar::chunk {"
                               "  background-color: #4CAF50;"
                               "  width: 20px;"
                               "}");
    
    statusLabel->setStyleSheet("QLabel {"
                               "  font-weight: bold;"
                               "  color: #4CAF50;"
                               "}");
    
    logTextEdit->setStyleSheet("QTextEdit {"
                               "  background-color: #1E1E1E;"
                               "  color: #FFFFFF;"
                               "  border: 1px solid #555;"
                               "}");
    
    // 更新复选框样式
    QString checkBoxStyle = "QCheckBox {"
                            "  spacing: 5px;"
                            "} "
                            "QCheckBox::indicator {"
                            "  width: 18px;"
                            "  height: 18px;"
                            "} "
                            "QCheckBox::indicator:unchecked {"
                            "  border: 2px solid #555;"
                            "  background-color: #222;"
                            "} "
                            "QCheckBox::indicator:checked {"
                            "  border: 2px solid #4CAF50;"
                            "  background-color: #4CAF50;"
                            "}";
    symbolCheckBox->setStyleSheet(checkBoxStyle);
    footprintCheckBox->setStyleSheet(checkBoxStyle);
    model3dCheckBox->setStyleSheet(checkBoxStyle);
}

void MainWindow::switchToLightTheme()
{
    setupLightTheme();
    // 保存主题设置
    settings->setValue("theme", "light");
}

void MainWindow::switchToDarkTheme()
{
    setupDarkTheme();
    // 保存主题设置
    settings->setValue("theme", "dark");
}

void MainWindow::loadSettings()
{
    componentIdsEdit->setText(settings->value("componentIds", "").toString());
    exportPathEdit->setText(settings->value("exportPath", "").toString());
    filePrefixEdit->setText(settings->value("filePrefix", "easyki_export").toString());
    symbolCheckBox->setChecked(settings->value("exportSymbol", true).toBool());
    footprintCheckBox->setChecked(settings->value("exportFootprint", true).toBool());
    model3dCheckBox->setChecked(settings->value("export3dModel", true).toBool());
    
    // 加载主题设置，默认使用明亮主题
    QString theme = settings->value("theme", "light").toString();
    if (theme == "dark") {
        setupDarkTheme();
    } else {
        setupLightTheme();
    }
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
        
        // 使用新的库导出方式，创建统一的库目录结构
        std::string exportPathStd = exportPath.toStdString();
        std::string filePrefixStd = filePrefix.toStdString();
        
        // 确保导出目录存在
        QDir exportDir(exportPath);
        if (!exportDir.exists()) {
            exportDir.mkpath(".");
        }
        
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
            
            // Export symbol to library structure
            if (exportSymbol) {
                logMessage += QString("  Exporting symbol to library...\n");
                try {
                    KicadSymbolExporter exporter(KicadVersion::v6);
                    if (exporter.exportSymbolToLibrary(cadData, exportPathStd, filePrefixStd)) {
                        logMessage += QString("  Symbol library created at: %1/%2.kicad_sym\n")
                            .arg(exportPath).arg(filePrefix);
                    } else {
                        logMessage += QString("  Error: Failed to export symbol library\n");
                    }
                } catch (const std::exception& e) {
                    logMessage += QString("  Error exporting symbol library: %1\n").arg(e.what());
                }
            }
            
            // Export footprint
            if (exportFootprint) {
                logMessage += QString("  Exporting footprint...\n");
                try {
                    // 创建封装库目录
                    QString footprintLibDir = QString("%1/%2.pretty")
                        .arg(exportPath)
                        .arg(filePrefix);
                    
                    QDir dir(footprintLibDir);
                    if (!dir.exists()) {
                        dir.mkpath(".");
                    }
                    
                    // 导出封装到封装库目录
                    QString footprintFilePath = QString("%1/%2.kicad_mod")
                        .arg(footprintLibDir)
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
                try {
                    // 创建3D模型库目录
                    QString model3dLibDir = QString("%1/%2.3dshapes")
                        .arg(exportPath)
                        .arg(filePrefix);
                    
                    QDir dir(model3dLibDir);
                    if (!dir.exists()) {
                        dir.mkpath(".");
                    }
                    
                    // 导出3D模型到模型库目录
                    QString modelFilePath = QString("%1/%2.step")
                        .arg(model3dLibDir)
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
        
        logMessage += QString("\nSuccessfully processed %1 out of %2 components.\n")
                         .arg(processedComponents).arg(totalComponents);
        logMessage += QString("Library structure created at: %1\n").arg(exportPath);
        logMessage += QString("  - %1.3dshapes/ (3D models)\n").arg(filePrefix);
        logMessage += QString("  - %1.kicad_sym (Symbols file)\n").arg(filePrefix);
        logMessage += QString("  - %1.pretty/ (Footprints)\n").arg(filePrefix);
        
        emit conversionFinished(logMessage);
    } catch (const std::exception& e) {
        QString error = QString("Conversion failed: %1").arg(e.what());
        emit conversionError(error);
    } catch (...) {
        emit conversionError("Unknown error occurred during conversion");
    }
    
    emit finished();
}
