#include "ComponentListItemData.h"

#include <QBuffer>

namespace EasyKiConverter {

ComponentListItemData::ComponentListItemData(const QString& componentId, QObject* parent)
    : QObject(parent)
    , m_componentId(componentId)
    , m_currentPreviewIndex(0)
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

QVariantList ComponentListItemData::previewImages() const {
    QVariantList result;
    for (const QImage& image : m_previewImages) {
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        result.append(QString::fromLatin1(byteArray.toBase64().data()));
    }
    return result;
}

void ComponentListItemData::addPreviewImage(const QImage& image) {
    if (!image.isNull()) {
        m_previewImages.append(image);
        // 如果是第一张图片，设置为主缩略图
        if (m_previewImages.size() == 1) {
            setThumbnail(image);
        }
        emit previewImagesChanged();
    }
}

void ComponentListItemData::insertPreviewImage(const QImage& image, int index) {
    if (image.isNull()) {
        return;
    }

    // 确保列表大小足够
    while (m_previewImages.size() <= index) {
        m_previewImages.append(QImage());
    }

    // 插入或替换指定位置的图片
    if (m_previewImages[index].isNull()) {
        m_previewImages[index] = image;
        qDebug() << "Inserted preview image at index:" << index;
    } else {
        // 如果该位置已有图片，替换它
        m_previewImages[index] = image;
        qDebug() << "Replaced preview image at index:" << index;
    }

    // 第一张预览图始终覆盖缩略图显示
    if (index == 0) {
        setThumbnail(image);
        qDebug() << "First preview image (index 0) set as thumbnail";
    } else if (!hasThumbnail()) {
        // 如果没有缩略图且不是第一张，使用当前图片作为缩略图
        setThumbnail(image);
    }

    emit previewImagesChanged();
}

void ComponentListItemData::setPreviewImages(const QList<QImage>& images) {
    m_previewImages = images;
    if (!images.isEmpty()) {
        setThumbnail(images.first());
        m_currentPreviewIndex = 0;
    }
    emit previewImagesChanged();
}

void ComponentListItemData::setCurrentPreviewIndex(int index) {
    if (index >= 0 && index < m_previewImages.size() && index != m_currentPreviewIndex) {
        m_currentPreviewIndex = index;
        setThumbnail(m_previewImages[index]);
        emit currentPreviewIndexChanged();
    }
}

void ComponentListItemData::nextPreviewImage() {
    if (!m_previewImages.isEmpty()) {
        int nextIndex = (m_currentPreviewIndex + 1) % m_previewImages.size();
        setCurrentPreviewIndex(nextIndex);
    }
}

void ComponentListItemData::previousPreviewImage() {
    if (!m_previewImages.isEmpty()) {
        int prevIndex = (m_currentPreviewIndex - 1 + m_previewImages.size()) % m_previewImages.size();
        setCurrentPreviewIndex(prevIndex);
    }
}

QString ComponentListItemData::datasheetUrl() const {
    if (m_componentData) {
        return m_componentData->datasheet();
    }
    return QString();
}

}  // namespace EasyKiConverter
