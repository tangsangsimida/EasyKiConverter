#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QThread>
#include <QStyleFactory>
#include <QGroupBox>
#include <QPropertyAnimation>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QProgressBar>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include "easyeda/EasyedaApi.h"
#include "kicad/KicadSymbolExporter.h"

class ConverterWorker : public QObject
{
    Q_OBJECT
    
public:
    explicit ConverterWorker(QObject *parent = nullptr);
    
public slots:
    void processConversion(const QStringList& componentIds,
                           const QString& filePrefix,
                           const QString& exportPath,
                           bool exportSymbol,
                           bool exportFootprint,
                           bool export3dModel);
    
signals:
    void progressUpdated(int value);
    void conversionFinished(const QString& message);
    void conversionError(const QString& error);
    void finished();
};

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void startConversion(const QStringList& componentIds,
                        const QString& filePrefix,
                        const QString& exportPath,
                        bool exportSymbol,
                        bool exportFootprint,
                        bool export3dModel);

private slots:
    void onConvertClicked();
    void onBrowseClicked();
    void onSelectAllClicked();
    void onClearClicked();
    void onPasteClicked();
    
    void onUpdateProgress(int value);
    void onExportFinished(const QString& message);
    void onConversionError(const QString& error);
    
    // 主题切换槽函数
    void switchToLightTheme();
    void switchToDarkTheme();

private:
    Ui::MainWindow *ui;
    QSettings* settings;
    QLineEdit* componentIdsEdit;
    QLineEdit* exportPathEdit;
    QLineEdit* filePrefixEdit;
    QPushButton* browseButton;
    QPushButton* convertButton;
    QPushButton* selectAllButton;
    QPushButton* clearButton;
    QPushButton* pasteButton;
    QCheckBox* symbolCheckBox;
    QCheckBox* footprintCheckBox;
    QCheckBox* model3dCheckBox;
    QProgressBar* progressBar;
    QLabel* statusLabel;
    QTextEdit* logTextEdit;
    ConverterWorker* worker;
    QThread* workerThread;
    
    // 主题菜单动作
    QAction* lightThemeAction;
    QAction* darkThemeAction;
    
    QStringList parseComponentIds(const QString& text) const;
    void setupUI();
    void loadSettings();
    void saveSettings();
    
    // 主题设置函数
    void setupLightTheme();
    void setupDarkTheme();
};

#endif // MAINWINDOW_H