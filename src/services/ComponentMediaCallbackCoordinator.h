#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

class ComponentService;

/**
 * @brief 协调元器件媒体和 LCSC 异步回调。
 * @details 负责预览图、数据手册及 LCSC 元数据回调的代次校验、状态合并和信号转发。
 */
class ComponentMediaCallbackCoordinator final {
public:
    /** @brief 处理单张预览图下载完成。 */
    static void handleImageReady(ComponentService& owner,
                                 const QString& componentId,
                                 const QByteArray& imageData,
                                 int imageIndex);

    /** @brief 处理 LCSC 元数据下载完成。 */
    static void handleLcscDataReady(ComponentService& owner,
                                    const QString& componentId,
                                    const QString& manufacturerPart,
                                    const QString& datasheetUrl,
                                    const QStringList& imageUrls);

    /** @brief 处理数据手册下载完成。 */
    static void handleDatasheetReady(ComponentService& owner,
                                     const QString& componentId,
                                     const QByteArray& datasheetData);

    /** @brief 处理预览图下载失败。 */
    static void handlePreviewImageError(ComponentService& owner, const QString& componentId, const QString& error);

    /** @brief 处理全部预览图下载完成。 */
    static void handleAllImagesReady(ComponentService& owner,
                                     const QString& componentId,
                                     const QStringList& imagePaths);
};

}  // namespace EasyKiConverter
