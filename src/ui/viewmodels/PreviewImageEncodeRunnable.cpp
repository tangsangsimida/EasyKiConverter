#include "PreviewImageEncodeRunnable.h"

#include "models/ComponentListItemData.h"

#include <QBuffer>

#include <utility>

namespace EasyKiConverter {

// 保存任务输入，避免后台执行时依赖列表模型的可变状态。
PreviewImageEncodeRunnable::PreviewImageEncodeRunnable(ComponentListItemData* item,
                                                       const QList<QImage>& images,
                                                       std::function<void(const QString&, const QStringList&)> callback)
    // 复制图像数据并接管回调，确保任务生命周期独立于视图模型。
    : m_componentId(item ? item->componentId() : QString()), m_images(images), m_callback(std::move(callback)) {}

// 将每张预览图编码为 PNG，再转换为 Base64 字符串交给模型线程处理。
void PreviewImageEncodeRunnable::run() {
    QStringList encodedList;
    for (const QImage& image : m_images) {
        if (image.isNull()) {
            encodedList.append(QString());
            continue;
        }

        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
            encodedList.append(QString());
            continue;
        }
        encodedList.append(QString::fromLatin1(byteArray.toBase64()));
    }

    if (m_callback) {
        m_callback(m_componentId, encodedList);
    }
}

}  // namespace EasyKiConverter
