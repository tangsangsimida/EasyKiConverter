#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QWidget>
#include <QThread>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QClipboard>
#include <QApplication>
#include <QTimer>
#include <QStringList>
#include <vector>
#include <string>

// 修复包含路径
#include "../easyeda/EasyedaApi.h"
#include "../kicad/KicadSymbolExporter.h"
#include "../easyeda/EasyedaImporter.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ConverterWorker;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConvertClicked();
    void onBrowseClicked();
    void onSelectAllClicked();
    void onClearClicked();
    void onPasteClicked();
    void onExportFinished(const QString& message);
    void onUpdateProgress(int value);
    void onConversionError(const QString& error);

signals:
    // 添加startConversion信号
    void startConversion(const QStringList& componentIds,
                        const QString& filePrefix,
                        const QString& exportPath,
                        bool exportSymbol,
                        bool exportFootprint,
                        bool export3dModel);

private:
    // UI elements
    QLineEdit* componentIdsEdit;
    QLineEdit* exportPathEdit;
    QLineEdit* filePrefixEdit;
    QTextEdit* logTextEdit;
    QPushButton* convertButton;
    QPushButton* browseButton;
    QPushButton* selectAllButton;
    QPushButton* clearButton;
    QPushButton* pasteButton;
    QProgressBar* progressBar;
    QCheckBox* symbolCheckBox;
    QCheckBox* footprintCheckBox;
    QCheckBox* model3dCheckBox;
    QLabel* statusLabel;
    
    // Worker thread
    ConverterWorker* worker;
    QThread* workerThread;
    
    // Settings
    QSettings* settings;
    
    // Methods
    void setupUI();
    void loadSettings();
    void saveSettings();
    QStringList parseComponentIds(const QString& text) const;
};

// Worker class for performing conversion in a separate thread
class ConverterWorker : public QObject
{
    Q_OBJECT

public:
    explicit ConverterWorker(QObject *parent = nullptr);

public slots:
    // 使用QStringList替代std::vector<std::string>
    void processConversion(const QStringList& componentIds, 
                          const QString& filePrefix, 
                          const QString& exportPath,
                          bool exportSymbol,
                          bool exportFootprint,
                          bool export3dModel);
    void moveToThread(QThread* thread) { QObject::moveToThread(thread); }

signals:
    void progressUpdated(int value);
    void conversionFinished(const QString& message);
    void conversionError(const QString& error);
    void finished();
};

#endif // MAINWINDOW_H