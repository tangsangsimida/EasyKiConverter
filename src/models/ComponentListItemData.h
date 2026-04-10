#ifndef COMPONENTLISTITEMDATA_H
#define COMPONENTLISTITEMDATA_H

#include "ComponentData.h"

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 元器件列表项数据模型
 *
 * 扩展 ComponentData，增加 UI 相关的状态信息
 */
class ComponentListItemData : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString componentId READ componentId CONSTANT)
    Q_PROPERTY(QString name READ name NOTIFY dataChanged)
    Q_PROPERTY(QString package READ package NOTIFY dataChanged)
    Q_PROPERTY(QVariantList previewImages READ previewImages NOTIFY previewImagesChanged)
    Q_PROPERTY(int previewImageCount READ previewImageCount NOTIFY previewImagesChanged)
    Q_PROPERTY(bool isValid READ isValid NOTIFY validationStatusChanged)
    Q_PROPERTY(bool isFetching READ isFetching NOTIFY fetchingStatusChanged)
    Q_PROPERTY(QString validationPhase READ validationPhase NOTIFY validationPhaseChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY validationStatusChanged)
    Q_PROPERTY(QString datasheetUrl READ datasheetUrl NOTIFY datasheetChanged)
    Q_PROPERTY(bool previewImageExported READ previewImageExported NOTIFY exportStatusChanged)
    Q_PROPERTY(bool datasheetExported READ datasheetExported NOTIFY exportStatusChanged)

public:
    explicit ComponentListItemData(const QString& componentId, QObject* parent = nullptr);

    // 批量更新预览图（暂停通知，更新完后统一通知）
    void setEncodedPreviewImagesHoldNotify(const QStringList& encodedImages);
    void setEncodedPreviewImagesSilent(const QStringList& encodedImages);
    static void holdPreviewImageNotifications();
    static void flushPreviewImageNotifications();

    // Getter 方法
    QString componentId() const {
        return m_componentId;
    }

    QString name() const {
        return m_name;
    }

    QString package() const {
        return m_package;
    }

    QVariantList previewImages() const;

    int previewImageCount() const {
        return m_previewImages.count();
    }

    bool isValid() const {
        return m_isValid;
    }

    bool isFetching() const {
        return m_isFetching;
    }

    QString validationPhase() const {
        return m_validationPhase;
    }

    QString errorMessage() const {
        return m_errorMessage;
    }

    QString datasheetUrl() const;

    bool previewImageExported() const {
        return m_previewImageExported;
    }

    bool datasheetExported() const {
        return m_datasheetExported;
    }

    QSharedPointer<ComponentData> componentData() const {
        return m_componentData;
    }

    // Setter 方法
    void setName(const QString& name);
    void setPackage(const QString& package);
    void setNameSilent(const QString& name);
    void setPackageSilent(const QString& package);
    void addPreviewImage(const QImage& image);
    void insertPreviewImage(const QImage& image, int index);
    void setPreviewImages(const QList<QImage>& images);
    void setComponentData(const QSharedPointer<ComponentData>& data);
    void setValid(bool valid);
    void setFetching(bool fetching);
    void setValidationPhase(const QString& phase);
    void setErrorMessage(const QString& error);
    void setPreviewImageExported(bool exported);
    void setDatasheetExported(bool exported);

    // 批量插入，不触发信号
    void insertPreviewImageSilent(const QImage& image, int index);
    // 批量插入完成后刷新缓存
    void finishPreviewImageLoading();
    // 设置预编码的图片
    void setEncodedPreviewImages(const QStringList& encodedImages);

    // 获取原始图片
    QList<QImage> previewImagesRaw() const {
        return m_previewImages;
    }

signals:
    void dataChanged();
    void previewImagesChanged();
    void validationStatusChanged();
    void fetchingStatusChanged();
    void validationPhaseChanged();
    void datasheetChanged();
    void exportStatusChanged();

private:
    void updatePreviewImagesCache() const;

    QString m_componentId;
    QString m_name;
    QString m_package;
    QList<QImage> m_previewImages;
    mutable QVariantList m_previewImagesCache;
    bool m_isValid;
    bool m_isFetching;
    QString m_validationPhase;  // "idle" | "validating" | "fetching_preview" | "completed" | "failed"
    QString m_errorMessage;
    QSharedPointer<ComponentData> m_componentData;
    bool m_previewImageExported = false;
    bool m_datasheetExported = false;

    // 静态标志：暂停预览图更新通知（用于批量更新）
    // 保护：s_holdPreviewImageNotifications 和 s_previewImageMutex 共同保护此标志
    static bool s_holdPreviewImageNotifications;
    static QMutex s_previewImageMutex;
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTITEMDATA_H
