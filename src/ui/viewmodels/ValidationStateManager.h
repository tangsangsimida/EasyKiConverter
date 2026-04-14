#ifndef VALIDATIONSTATEMANAGER_H
#define VALIDATIONSTATEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

class ValidationStateManager : public QObject {
    Q_OBJECT
public:
    explicit ValidationStateManager(QObject* parent = nullptr);
    ~ValidationStateManager() override;

    void startValidation(int count);
    void addValidation(int count);
    void onComponentValidated(const QString& componentId);
    void onComponentFailed(const QString& componentId);
    void reset();

    bool isAllDone() const;
    bool isPreviewFetchEnabled() const;

    int pendingCount() const;
    int validatedCount() const;
    QStringList validatedComponentIds() const;

signals:
    void validationCompleted(const QStringList& validatedIds);
    void validationStateChanged(int pendingCount);

private:
    void checkAndNotifyCompletion();

private:
    int m_pendingValidationCount;
    int m_totalValidationCount;  // 记录需要验证的总数
    QStringList m_validatedComponentIds;
    QStringList m_failedComponentIds;
    bool m_previewFetchEnabled{false};
};

}  // namespace EasyKiConverter

#endif  // VALIDATIONSTATEMANAGER_H
