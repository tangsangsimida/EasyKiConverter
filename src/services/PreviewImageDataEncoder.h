#ifndef PREVIEWIMAGEDATAENCODER_H
#define PREVIEWIMAGEDATAENCODER_H

#include <QByteArray>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief 预览图文件编码结果
 *
 * encodedImages 与 imageData 按预览图文件名中的序号排列，缺失的序号保留为空元素。
 */
struct PreviewImageDataResult {
    QStringList encodedImages;
    QList<QByteArray> imageData;
};

/**
 * @brief 将缓存中的预览图文件读取并编码为内存数据
 *
 * 该类只负责文件名序号解析和文件读取，不访问网络、不修改组件状态，也不发送 Qt 信号。
 */
class PreviewImageDataEncoder final {
public:
    /** @brief 从预览图文件名解析图片序号。 */
    static int imageIndexFromPath(const QString& path);

    /** @brief 读取文件并生成 Base64 字符串与原始数据。 */
    static PreviewImageDataResult encodeFiles(const QStringList& imagePaths);
};

}  // namespace EasyKiConverter

#endif  // PREVIEWIMAGEDATAENCODER_H
