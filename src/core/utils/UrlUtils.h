#pragma once

#include <QString>
#include <QStringList>

namespace EasyKiConverter::UrlUtils {

/**
 * @brief 标准化预览图 URL
 *
 * 处理各种 URL 格式（相对路径、协议相对路径、第三方 CDN 等），
 * 转换为标准的 https:// URL。
 *
 * @param imageUrl 原始 URL
 * @return 标准化后的 URL
 */
QString normalizePreviewImageUrl(const QString& imageUrl);

/**
 * @brief 去重并标准化 URL 列表
 *
 * 对 URL 列表进行标准化，然后去除重复项。
 *
 * @param imageUrls 原始 URL 列表
 * @return 去重后的标准化 URL 列表
 */
QStringList deduplicateAndNormalizeUrls(const QStringList& imageUrls);

}  // namespace EasyKiConverter::UrlUtils
