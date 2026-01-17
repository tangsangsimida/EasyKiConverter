#ifndef EXPORTSETTINGSVIEWMODEL_H
#define EXPORTSETTINGSVIEWMODEL_H

#include <QObject>
#include <QString>
#include "src/services/ConfigService.h"

namespace EasyKiConverter
{

    /**
     * @brief 导出设置视图模型类
     *
     * 负责管理导出设置相关的 UI 状态和操作
     */
    class ExportSettingsViewModel : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(QString outputPath READ outputPath WRITE setOutputPath NOTIFY outputPathChanged)
        Q_PROPERTY(QString libName READ libName WRITE setLibName NOTIFY libNameChanged)
        Q_PROPERTY(bool exportSymbol READ exportSymbol WRITE setExportSymbol NOTIFY exportSymbolChanged)
        Q_PROPERTY(bool exportFootprint READ exportFootprint WRITE setExportFootprint NOTIFY exportFootprintChanged)
        Q_PROPERTY(bool exportModel3D READ exportModel3D WRITE setExportModel3D NOTIFY exportModel3DChanged)
        Q_PROPERTY(bool overwriteExistingFiles READ overwriteExistingFiles WRITE setOverwriteExistingFiles NOTIFY overwriteExistingFilesChanged)

    public:
        explicit ExportSettingsViewModel(QObject *parent = nullptr);
        ~ExportSettingsViewModel() override;

        // Getter 方法
        QString outputPath() const { return m_outputPath; }
        QString libName() const { return m_libName; }
        bool exportSymbol() const { return m_exportSymbol; }
        bool exportFootprint() const { return m_exportFootprint; }
        bool exportModel3D() const { return m_exportModel3D; }
        bool overwriteExistingFiles() const { return m_overwriteExistingFiles; }
        bool isExporting() const { return m_isExporting; }
        int progress() const { return m_progress; }
        QString status() const { return m_status; }

        // Setter 方法
        void setOutputPath(const QString &path);
        void setLibName(const QString &name);
        void setExportSymbol(bool enabled);
        void setExportFootprint(bool enabled);
        void setExportModel3D(bool enabled);
        void setOverwriteExistingFiles(bool enabled);

    public slots:
        Q_INVOKABLE void saveConfig();
        Q_INVOKABLE void resetConfig();
        Q_INVOKABLE void startExport(const QStringList &componentIds);
        Q_INVOKABLE void cancelExport();

    signals:
        void outputPathChanged();
        void libNameChanged();
        void exportSymbolChanged();
        void exportFootprintChanged();
        void exportModel3DChanged();
        void overwriteExistingFilesChanged();
        void isExportingChanged();
        void progressChanged();
        void statusChanged();

    private:
        void loadFromConfig();

    private slots:
        void handleExportProgress(int current, int total);
        void handleComponentExported(const QString &componentId, bool success, const QString &message);
        void handleExportCompleted(bool success);
        void handleExportFailed(const QString &error);

    private:
        void setIsExporting(bool exporting);
        void setProgress(int progress);
        void setStatus(const QString &status);

        ConfigService *m_configService;
        QString m_outputPath;
        QString m_libName;
        bool m_exportSymbol;
        bool m_exportFootprint;
        bool m_exportModel3D;
        bool m_overwriteExistingFiles;
        bool m_isExporting;
        int m_progress;
        QString m_status;
    };

} // namespace EasyKiConverter

#endif // EXPORTSETTINGSVIEWMODEL_H
