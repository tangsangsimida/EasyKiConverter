#include "ComponentListItemData.h"

#include <QBuffer>

namespace EasyKiConverter {

ComponentListItemData::ComponentListItemData(const QString& componentId, QObject* parent)
    : QObject(parent)
    , m_componentId(componentId)
    , m_isValid(true)  // 默认为 true，直到验证失败
    , m_isFetching(false) {}

void ComponentListItemData::setName(const QString& name) {
    if (m_name != name) {
        m_name = name;
        emit dataChanged();
    }
}

void ComponentListItemData::setPackage(const QString& package) {
    if (m_package != package) {
        m_package = package;
        emit dataChanged();
    }
}

void ComponentListItemData::setThumbnail(const QImage& thumbnail) {
    m_thumbnail = thumbnail;
    m_thumbnailBase64Cache.clear();  // 清除缓存，下次访问时重新生成
    emit thumbnailChanged();
}

QString ComponentListItemData::thumbnailBase64() const {
    if (m_thumbnail.isNull()) {
        return QString();
    }

    // 使用缓存避免每次 QML 访问都重新编码
    if (m_thumbnailBase64Cache.isEmpty()) {
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        m_thumbnail.save(&buffer, "PNG");
        m_thumbnailBase64Cache = QString::fromLatin1(byteArray.toBase64().data());
    }

    return m_thumbnailBase64Cache;
}

void ComponentListItemData::setComponentData(const QSharedPointer<ComponentData>& data) {
    m_componentData = data;
    if (data) {
        if (!data->name().isEmpty())
            setName(data->name());
        if (!data->package().isEmpty())
            setPackage(data->package());
    }
    emit dataChanged();
}

void ComponentListItemData::setValid(bool valid) {
    if (m_isValid != valid) {
        m_isValid = valid;
        emit validationStatusChanged();
    }
}

void ComponentListItemData::setFetching(bool fetching) {
    if (m_isFetching != fetching) {
        m_isFetching = fetching;
        emit fetchingStatusChanged();
    }
}

void ComponentListItemData::setErrorMessage(const QString& error) {
    if (m_errorMessage != error) {
        m_errorMessage = error;
        emit validationStatusChanged();
    }
}

}  // namespace EasyKiConverter
