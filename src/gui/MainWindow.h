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
#include <QListWidget>
#include "easyeda/EasyedaApi.h"
#include "kicad/KicadSymbolExporter.h"
#include "ComponentManager.h"

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
    void onAddComponentClicked();
    void onRemoveComponentClicked();
    
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
    QPushButton* addComponentButton;
    QPushButton* removeComponentButton;
    QCheckBox* symbolCheckBox;
    QCheckBox* footprintCheckBox;
    QCheckBox* model3dCheckBox;
    QProgressBar* progressBar;
    QLabel* statusLabel;
    QTextEdit* logTextEdit;
    QListWidget* componentListWidget;  // 新增组件列表
    ConverterWorker* worker;
    QThread* workerThread;
    
    // 组件管理器
    ComponentManager* componentManager;
    
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
    
    // 组件列表相关
    void updateComponentList();
};

#endif // MAINWINDOW_H