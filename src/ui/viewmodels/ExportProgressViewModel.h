#ifndef EXPORTPROGRESSVIEWMODEL_H
#define EXPORTPROGRESSVIEWMODEL_H

#include <QObject>
#include <QString>
#include "src/services/ExportService.h"
#include "src/services/ComponentService.h"

namespace EasyKiConverter
{

    /**
     * @brief 导出进度视图模型类
     *
     * 负责管理导出进度和结果相关的 UI 状态
     */
    class ExportProgressViewModel : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
        Q_PROPERTY(QString status READ status NOTIFY statusChanged)
        Q_PROPERTY(bool isExporting READ isExporting NOTIFY isExportingChanged)
        Q_PROPERTY(int successCount READ successCount NOTIFY successCountChanged)
        Q_PROPERTY(int failureCount READ failureCount NOTIFY failureCountChanged)

    public:
        explicit ExportProgressViewModel(ExportService *exportService, ComponentService *componentService, QObject *parent = nullptr);
        ~ExportProgressViewModel() override;

        // Getter 方法
        int progress() const { return m_progress; }
        QString status() const { return m_status; }
        bool isExporting() const { return m_isExporting; }
        int successCount() const { return m_successCount; }
        int failureCount() const { return m_failureCount; }

    public slots:
        Q_INVOKABLE void startExport(const QStringList &componentIds, const QString &outputPath, const QString &libName, bool exportSymbol, bool exportFootprint, bool exportModel3D, bool overwriteExistingFiles);
        Q_INVOKABLE void cancelExport();

    signals:
        void progressChanged();
        void statusChanged();
        void isExportingChanged();
        void successCountChanged();
        void failureCountChanged();
        void exportCompleted(int totalCount, int successCount);
        void componentExported(const QString &componentId, bool success, const QString &message);

    private slots:
        void handleExportProgress(int current, int total);
        void handleExportCompleted(int totalCount, int successCount);
        void handleExportFailed(const QString &error);
        void handleComponentExported(const QString &componentId, bool success, const QString &message);
        void handleComponentDataFetched(const QString &componentId, const ComponentData &data);

    private:
        ExportService *m_exportService;
        ComponentService *m_componentService;
        int m_progress;
        QString m_status;
        bool m_isExporting;
        int m_successCount;
        int m_failureCount;
        QStringList m_componentIds;
        int m_fetchedCount;
        QList<ComponentData> m_collectedData;
        ExportOptions m_exportOptions;
    };

} // namespace EasyKiConverter

#endif // EXPORTPROGRESSVIEWMODEL_H
