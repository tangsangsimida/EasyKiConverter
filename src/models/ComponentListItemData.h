#ifndef COMPONENTLISTITEMDATA_H
#define COMPONENTLISTITEMDATA_H

#include "ComponentData.h"

#include <QImage>
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
    Q_PROPERTY(QImage thumbnail READ thumbnail NOTIFY thumbnailChanged)
    Q_PROPERTY(QString thumbnailBase64 READ thumbnailBase64 NOTIFY thumbnailChanged)
    Q_PROPERTY(bool isValid READ isValid NOTIFY validationStatusChanged)
    Q_PROPERTY(bool isFetching READ isFetching NOTIFY fetchingStatusChanged)
    Q_PROPERTY(bool hasThumbnail READ hasThumbnail NOTIFY thumbnailChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY validationStatusChanged)

public:
    explicit ComponentListItemData(const QString& componentId, QObject* parent = nullptr);

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
    QImage thumbnail() const {
        return m_thumbnail;
    }
    QString thumbnailBase64() const;
    bool isValid() const {
        return m_isValid;
    }
    bool isFetching() const {
        return m_isFetching;
    }
    bool hasThumbnail() const {
        return !m_thumbnail.isNull();
    }
    QString errorMessage() const {
        return m_errorMessage;
    }
    QSharedPointer<ComponentData> componentData() const {
        return m_componentData;
    }

    // Setter 方法
    void setName(const QString& name);
    void setPackage(const QString& package);
    void setThumbnail(const QImage& thumbnail);
    void setComponentData(const QSharedPointer<ComponentData>& data);
    void setValid(bool valid);
    void setFetching(bool fetching);
    void setErrorMessage(const QString& error);

signals:
    void dataChanged();
    void thumbnailChanged();
    void validationStatusChanged();
    void fetchingStatusChanged();

private:
    QString m_componentId;
    QString m_name;
    QString m_package;
    QImage m_thumbnail;
    mutable QString m_thumbnailBase64Cache;  // 缓存 base64 编码结果，避免重复计算
    bool m_isValid;
    bool m_isFetching;
    QString m_errorMessage;
    QSharedPointer<ComponentData> m_componentData;
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTITEMDATA_H
