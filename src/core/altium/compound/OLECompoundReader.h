#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief OLE 复合文档 V3 只读解析器
 * @details 解析 CFB/OLE Structured Storage 文件中的存储树、普通流和迷你流。
 *          该类只负责读取，不会修改输入文件，供 Altium 库校验和后续增量合并使用。
 *
 * 参考：[MS-CFB] Compound File Binary File Format。
 */
class OLECompoundReader {
public:
    OLECompoundReader();
    ~OLECompoundReader();

    /**
     * @brief 从文件加载 OLE 复合文档
     * @param filePath 输入文件路径
     * @return 文件格式有效且所有目录流可读取时返回 true
     */
    bool open(const QString& filePath);

    /**
     * @brief 获取读取过程中是否发生错误
     * @return true 表示最近一次打开失败
     */
    bool hasError() const;

    /**
     * @brief 获取最近一次错误说明
     * @return 错误说明；没有错误时为空
     */
    QString errorString() const;

    /**
     * @brief 获取文件中的全部流路径
     * @return 使用 / 分隔的、相对于 Root Entry 的流路径
     */
    QStringList streamPaths() const;

    /**
     * @brief 判断指定流是否存在
     * @param streamPath 相对于 Root Entry 的流路径
     * @return 流存在时返回 true
     */
    bool containsStream(const QString& streamPath) const;

    /**
     * @brief 读取指定流
     * @param streamPath 相对于 Root Entry 的流路径
     * @param data 输出流数据
     * @return 流存在且 data 指针有效时返回 true
     */
    bool readStream(const QString& streamPath, QByteArray* data) const;

private:
    void clear();
    bool fail(const QString& message);

    bool m_opened = false;
    QString m_errorMessage;
    QHash<QString, QByteArray> m_streams;
};

}  // namespace EasyKiConverter
