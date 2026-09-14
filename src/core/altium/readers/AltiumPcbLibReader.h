#pragma once

#include "core/altium/compound/OLECompoundReader.h"

#include <QMap>
#include <QVector>

namespace EasyKiConverter {

/**
 * @brief Altium PcbLib 库级读取器
 * @details 解析 FileHeader、Library/Data、SectionKeys 以及封装 Storage 映射。
 *          当前只读，不执行增量写回。
 */
class AltiumPcbLibReader {
public:
    /** @brief PcbLib 封装目录项 */
    struct ComponentInfo {
        QString name;
        QString sectionKey;
    };

    /**
     * @brief 打开并解析 PcbLib 文件
     * @param filePath 文件路径
     * @return 结构完整且封装映射有效时返回 true
     */
    bool open(const QString& filePath);

    /** @brief 获取封装目录 */
    QVector<ComponentInfo> components() const;
    /** @brief 获取 PcbLib 文件版本文本 */
    QString fileVersion() const;
    /** @brief 获取 Library/Data 中的元数据块 */
    QByteArray libraryMetadata() const;
    /** @brief 读取指定封装的指定流 */
    bool readFootprintStream(int componentIndex, const QString& streamName, QByteArray* data) const;
    /** @brief 读取指定封装的指定流 */
    bool readFootprintStream(const QString& componentName, const QString& streamName, QByteArray* data) const;
    /** @brief 获取最近一次错误说明 */
    QString errorString() const;
    /** @brief 获取是否发生读取错误 */
    bool hasError() const;

private:
    bool fail(const QString& message);
    int componentIndex(const QString& componentName) const;

    OLECompoundReader m_oleReader;
    QVector<ComponentInfo> m_components;
    QString m_fileVersion;
    QByteArray m_libraryMetadata;
    QString m_errorMessage;
};

}  // namespace EasyKiConverter
