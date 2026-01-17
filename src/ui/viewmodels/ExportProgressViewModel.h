#ifndef EXPORTPROGRESSVIEWMODEL_H
#define EXPORTPROGRESSVIEWMODEL_H

#include <QObject>
#include <QString>
#include "src/services/ExportService.h"

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
        explicit ExportProgressViewModel(ExportService *service, QObject *parent = nullptr);
        ~ExportProgressViewModel() override;

        // Getter 方法
        int progress() const { return m_progress; }
        QString status() const { return m_status; }
        bool isExporting() const { return m_isExporting; }
        int successCount() const { return m_successCount; }
        int failureCount() const { return m_failureCount; }

    public slots:
        Q_INVOKABLE void startExport(const QStringList &componentIds, const ExportOptions &options);
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

    private:
        ExportService *m_service;
        int m_progress;
        QString m_status;
        bool m_isExporting;
        int m_successCount;
        int m_failureCount;
    };

} // namespace EasyKiConverter

#endif // EXPORTPROGRESSVIEWMODEL_H
