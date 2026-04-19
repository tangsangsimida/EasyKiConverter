#include "ComponentListItemData.h"

#include <QBuffer>

namespace EasyKiConverter {

// 静态成员初始化
bool ComponentListItemData::s_holdPreviewImageNotifications = false;
QMutex ComponentListItemData::s_previewImageMutex;

ComponentListItemData::ComponentListItemData(const QString& componentId, QObject* parent)
    : QObject(parent)
    , m_componentId(componentId)
    , m_isValid(true)  // 默认为 true，直到验证失败
    , m_isFetching(false)
    , m_validationPhase("idle") {}

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

void ComponentListItemData::setNameSilent(const QString& name) {
    m_name = name;
}

void ComponentListItemData::setPackageSilent(const QString& package) {
    m_package = package;
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

void ComponentListItemData::setValidationPhase(const QString& phase) {
    if (m_validationPhase != phase) {
        m_validationPhase = phase;
        emit validationPhaseChanged();
    }
}

void ComponentListItemData::setErrorMessage(const QString& error) {
    if (m_errorMessage != error) {
        m_errorMessage = error;
        emit validationStatusChanged();
    }
}

void ComponentListItemData::setRetryable(bool retryable) {
    if (m_retryable != retryable) {
        m_retryable = retryable;
        emit validationStatusChanged();
    }
}

void ComponentListItemData::setPreviewImageExported(bool exported) {
    if (m_previewImageExported != exported) {
        m_previewImageExported = exported;
        emit exportStatusChanged();
    }
}

void ComponentListItemData::setDatasheetExported(bool exported) {
    if (m_datasheetExported != exported) {
        m_datasheetExported = exported;
        emit exportStatusChanged();
    }
}

void ComponentListItemData::updatePreviewImagesCache() const {
    m_previewImagesCache.clear();
    for (const QImage& image : m_previewImages) {
        if (image.isNull()) {
            m_previewImagesCache.append(QVariant());
            continue;
        }
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        m_previewImagesCache.append(QString::fromLatin1(byteArray.toBase64().data()));
    }
}

QVariantList ComponentListItemData::previewImages() const {
    if (m_previewImagesCache.isEmpty() && !m_previewImages.isEmpty()) {
        updatePreviewImagesCache();
    }
    return m_previewImagesCache;
}

int ComponentListItemData::previewImageCount() const {
    int cacheCount = 0;
    for (const QVariant& cached : m_previewImagesCache) {
        if (cached.isValid() && !cached.toString().isEmpty()) {
            ++cacheCount;
        }
    }

    int rawCount = 0;
    for (const QImage& image : m_previewImages) {
        if (!image.isNull()) {
            ++rawCount;
        }
    }

    return qMax(cacheCount, rawCount);
}

void ComponentListItemData::addPreviewImage(const QImage& image) {
    if (!image.isNull()) {
        m_previewImages.append(image);
        m_previewImagesCache.clear();
        emit previewImagesChanged();
    }
}

void ComponentListItemData::insertPreviewImage(const QImage& image, int index) {
    if (image.isNull()) {
        return;
    }

    while (m_previewImages.size() <= index) {
        m_previewImages.append(QImage());
    }

    if (m_previewImages[index].isNull()) {
        m_previewImages[index] = image;
        qDebug() << "Inserted preview image at index:" << index;
    } else {
        m_previewImages[index] = image;
        qDebug() << "Replaced preview image at index:" << index;
    }

    m_previewImagesCache.clear();
    emit previewImagesChanged();
}

void ComponentListItemData::insertPreviewImageSilent(const QImage& image, int index) {
    if (image.isNull()) {
        return;
    }

    while (m_previewImages.size() <= index) {
        m_previewImages.append(QImage());
    }

    m_previewImages[index] = image;

    // 注意：不在这里更新缓存和发射信号
    // 缓存更新和信号发射由 ComponentListViewModel 的 batchUpdatePreviewImages 统一处理
    // 这样可以避免在主线程中进行耗时的图片编码操作
}

// 编码单张图片
static QString encodeImageToBase64(const QImage& image) {
    if (image.isNull()) {
        return QString();
    }
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QString::fromLatin1(byteArray.toBase64().data());
}

void ComponentListItemData::finishPreviewImageLoading() {
    m_previewImagesCache.clear();
    for (const QImage& image : m_previewImages) {
        if (image.isNull()) {
            m_previewImagesCache.append(QVariant());
            continue;
        }
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        m_previewImagesCache.append(QString::fromLatin1(byteArray.toBase64().data()));
    }
    emit previewImagesChanged();
}

void ComponentListItemData::setEncodedPreviewImages(const QStringList& encodedImages) {
    m_previewImagesCache.clear();
    for (const QString& encoded : encodedImages) {
        if (encoded.isEmpty()) {
            m_previewImagesCache.append(QVariant());
        } else {
            m_previewImagesCache.append(encoded);
        }
    }
    emit previewImagesChanged();
}

void ComponentListItemData::setEncodedPreviewImageAt(const QString& encodedImage, int index, bool notify) {
    if (index < 0) {
        return;
    }

    while (m_previewImagesCache.size() <= index) {
        m_previewImagesCache.append(QVariant());
    }

    if (encodedImage.isEmpty()) {
        m_previewImagesCache[index] = QVariant();
    } else {
        m_previewImagesCache[index] = encodedImage;
    }

    if (notify) {
        emit previewImagesChanged();
    }
}

void ComponentListItemData::notifyPreviewImagesChanged() {
    emit previewImagesChanged();
}

void ComponentListItemData::setPreviewImages(const QList<QImage>& images) {
    m_previewImages = images;
    m_previewImagesCache.clear();
    emit previewImagesChanged();
}

QString ComponentListItemData::datasheetUrl() const {
    if (m_componentData) {
        return m_componentData->datasheet();
    }
    return QString();
}

void ComponentListItemData::setEncodedPreviewImagesHoldNotify(const QStringList& encodedImages) {
    m_previewImagesCache.clear();
    for (const QString& encoded : encodedImages) {
        if (encoded.isEmpty()) {
            m_previewImagesCache.append(QVariant());
        } else {
            m_previewImagesCache.append(encoded);
        }
    }
    // 如果没有暂停通知，则发射信号
    bool shouldNotify;
    {
        QMutexLocker locker(&s_previewImageMutex);
        shouldNotify = !s_holdPreviewImageNotifications;
    }
    if (shouldNotify) {
        emit previewImagesChanged();
    }
}

void ComponentListItemData::setEncodedPreviewImagesSilent(const QStringList& encodedImages) {
    m_previewImagesCache.clear();
    for (const QString& encoded : encodedImages) {
        if (encoded.isEmpty()) {
            m_previewImagesCache.append(QVariant());
        } else {
            m_previewImagesCache.append(encoded);
        }
    }
    // 不发射信号，由其他更新触发 UI 刷新
}

void ComponentListItemData::holdPreviewImageNotifications() {
    QMutexLocker locker(&s_previewImageMutex);
    s_holdPreviewImageNotifications = true;
}

void ComponentListItemData::flushPreviewImageNotifications() {
    // 静态方法无法发射实例信号，改为发射所有待更新项的信号
    // 使用锁保护确保线程安全
    QMutexLocker locker(&s_previewImageMutex);
    s_holdPreviewImageNotifications = false;
}

}  // namespace EasyKiConverter
