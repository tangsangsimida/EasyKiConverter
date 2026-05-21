#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

class QUrl;

namespace EasyKiConverter {

enum class ResourceType;

namespace UrlUtils {

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

/**
 * @brief 验证 URL 是否在允许的白名单内
 *
 * 检查 URL 的 scheme（仅允许 https）和 host（按 ResourceType 限制）。
 * 用于在 NetworkClient 发送请求前统一校验，防止访问非预期主机。
 *
 * @param url 要验证的 URL
 * @param resourceType 资源类型，用于确定允许的 host 集合
 * @param errorMessage 输出参数，校验失败时的错误信息
 * @return 如果 URL 合法则返回 true
 */
bool isAllowedUrl(const QUrl& url, ResourceType resourceType, QString* errorMessage = nullptr);

}  // namespace UrlUtils
}  // namespace EasyKiConverter
