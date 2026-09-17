#ifndef PREVIEWIMAGEENCODERUNNABLE_H
#define PREVIEWIMAGEENCODERUNNABLE_H

#include <QImage>
#include <QRunnable>
#include <QString>
#include <QStringList>

#include <functional>

namespace EasyKiConverter {

class ComponentListItemData;

/**
 * @brief 在线程池中编码元器件预览图。
 *
 * 该任务只负责将图像编码为 Base64 PNG，不直接访问或修改列表模型状态。
 */
class PreviewImageEncodeRunnable final : public QRunnable {
public:
    /**
     * @brief 创建预览图编码任务。
     * @param item 用于取得元器件编号的列表项。
     * @param images 待编码的预览图列表。
     * @param callback 编码完成后的回调。
     */
    PreviewImageEncodeRunnable(ComponentListItemData* item,
                               const QList<QImage>& images,
                               std::function<void(const QString&, const QStringList&)> callback);

    /**
     * @brief 执行预览图编码并回调结果。
     */
    void run() override;

private:
    QString m_componentId;
    QList<QImage> m_images;
    std::function<void(const QString&, const QStringList&)> m_callback;
};

}  // namespace EasyKiConverter

#endif  // PREVIEWIMAGEENCODERUNNABLE_H
