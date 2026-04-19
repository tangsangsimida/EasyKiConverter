#pragma once

#include "services/ConfigService.h"
#include "services/export/ParallelExportService.h"

#include <QObject>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 导出设置视图模型
 *
 * 负责管理导出设置相关的 UI 状态和操作
 */
class ExportSettingsViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString outputPath READ outputPath WRITE setOutputPath NOTIFY outputPathChanged)
    Q_PROPERTY(QString libName READ libName WRITE setLibName NOTIFY libNameChanged)
    Q_PROPERTY(bool exportSymbol READ exportSymbol WRITE setExportSymbol NOTIFY exportSymbolChanged)
    Q_PROPERTY(bool exportFootprint READ exportFootprint WRITE setExportFootprint NOTIFY exportFootprintChanged)
    Q_PROPERTY(bool exportModel3D READ exportModel3D WRITE setExportModel3D NOTIFY exportModel3DChanged)
    Q_PROPERTY(
        int exportModel3DFormat READ exportModel3DFormat WRITE setExportModel3DFormat NOTIFY exportModel3DFormatChanged)
    Q_PROPERTY(bool exportPreviewImages READ exportPreviewImages WRITE setExportPreviewImages NOTIFY
                   exportPreviewImagesChanged)
    Q_PROPERTY(bool exportDatasheet READ exportDatasheet WRITE setExportDatasheet NOTIFY exportDatasheetChanged)
    Q_PROPERTY(bool overwriteExistingFiles READ overwriteExistingFiles WRITE setOverwriteExistingFiles NOTIFY
                   overwriteExistingFilesChanged)
    Q_PROPERTY(
        bool weakNetworkSupport READ weakNetworkSupport WRITE setWeakNetworkSupport NOTIFY weakNetworkSupportChanged)
    Q_PROPERTY(int exportMode READ exportMode WRITE setExportMode NOTIFY exportModeChanged)
    Q_PROPERTY(bool debugMode READ debugMode WRITE setDebugMode NOTIFY debugModeChanged)
    Q_PROPERTY(bool isExporting READ isExporting NOTIFY isExportingChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit ExportSettingsViewModel(ParallelExportService* exportService, QObject* parent = nullptr);
    ~ExportSettingsViewModel() override;

    // Getter 方法
    QString outputPath() const {
        return m_outputPath;
    }

    QString libName() const {
        return m_libName;
    }

    bool exportSymbol() const {
        return m_exportSymbol;
    }

    bool exportFootprint() const {
        return m_exportFootprint;
    }

    bool exportModel3D() const {
        return m_exportModel3D;
    }

    // 3D模型格式(位掩码): 0=NONE, 1=WRL, 2=STEP, 3=BOTH
    int exportModel3DFormat() const {
        return m_exportModel3DFormat;
    }

    bool exportPreviewImages() const {
        return m_exportPreviewImages;
    }

    bool exportDatasheet() const {
        return m_exportDatasheet;
    }

    bool overwriteExistingFiles() const {
        return m_overwriteExistingFiles;
    }

    bool weakNetworkSupport() const {
        return m_weakNetworkSupport;
    }

    int exportMode() const {
        return m_exportMode;
    }

    bool debugMode() const {
        return m_debugMode;
    }

    bool isExporting() const {
        return m_isExporting;
    }

    QString status() const {
        return m_status;
    }

    // Setter 方法（标记为 Q_INVOKABLE 以便QML 中调用）
    Q_INVOKABLE void setOutputPath(const QString& path);
    Q_INVOKABLE void setLibName(const QString& name);
    Q_INVOKABLE void setExportSymbol(bool enabled);
    Q_INVOKABLE void setExportFootprint(bool enabled);
    Q_INVOKABLE void setExportModel3D(bool enabled);
    Q_INVOKABLE void setExportModel3DFormat(int format);
    Q_INVOKABLE void setExportPreviewImages(bool enabled);
    Q_INVOKABLE void setExportDatasheet(bool enabled);
    Q_INVOKABLE void setOverwriteExistingFiles(bool enabled);
    Q_INVOKABLE void setWeakNetworkSupport(bool enabled);
    Q_INVOKABLE void setExportMode(int mode);
    Q_INVOKABLE void setDebugMode(bool enabled);

public slots:
    Q_INVOKABLE void saveConfig();
    Q_INVOKABLE void resetConfig();
    Q_INVOKABLE void startExport(const QStringList& componentIds);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE bool openOutputFolder();

signals:
    void outputPathChanged();
    void libNameChanged();
    void exportSymbolChanged();
    void exportFootprintChanged();
    void exportModel3DChanged();
    void exportModel3DFormatChanged();
    void exportPreviewImagesChanged();
    void exportDatasheetChanged();
    void overwriteExistingFilesChanged();
    void weakNetworkSupportChanged();
    void exportModeChanged();
    void debugModeChanged();
    void isExportingChanged();
    void statusChanged();
    void preloadStarted();
    void exportStarted();

private slots:
    void handlePreloadProgressChanged(const PreloadProgress& progress);
    void handlePreloadCompleted(int successCount, int failedCount);
    void handleProgressChanged(const ExportOverallProgress& progress);
    void handleTypeCompleted(const QString& typeName, int successCount, int failedCount, int skippedCount);
    void handleCompleted(int successCount, int failedCount);
    void handleCancelled();
    void handleFailed(const QString& error);

private:
    void buildExportOptions();
    void loadFromConfig();
    void setIsExporting(bool exporting);
    void setStatus(const QString& status);

private:
    ParallelExportService* m_exportService;
    ConfigService* m_configService;
    QString m_outputPath;
    QString m_libName;
    bool m_exportSymbol;
    bool m_exportFootprint;
    bool m_exportModel3D;
    int m_exportModel3DFormat;  // bitmask: 1=WRL, 2=STEP, 3=Both
    bool m_exportPreviewImages;
    bool m_exportDatasheet;
    bool m_overwriteExistingFiles;
    bool m_weakNetworkSupport;
    int m_exportMode;  // 0 = 追加模式, 1 = 更新模式
    bool m_debugMode;
    bool m_isExporting;
    QString m_status;
    QStringList m_pendingComponentIds;
};

}  // namespace EasyKiConverter
