#pragma once

#include "core/altium/compound/OLECompoundReader.h"

#include <QByteArray>
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

    /** @brief PcbLib Data 中的已知图元块 */
    struct PrimitiveBlock {
        quint8 flags = 0;
        QByteArray payload;
        QByteArray encoded;
    };

    /**
     * @brief PcbLib Data 中的图元对象
     * @details 保留对象 ID、所有子块和完整编码；未知对象类型不会被猜测解析。
     */
    struct PrimitiveRecord {
        quint8 objectId = 0;
        quint8 layer = 0;  ///< 图元公共头部中的层编号
        quint16 primitiveFlags = 0;  ///< 图元公共头部中的标志位
        QVector<PrimitiveBlock> blocks;
        QByteArray encoded;
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
    /**
     * @brief 扫描指定封装 Data 流中的已知图元对象
     * @details Data 流首部的封装名称不会作为对象返回。当前支持项目写入器使用的对象类型；
     *          遇到未知对象 ID 或损坏子块时返回 false，调用方仍可通过 readFootprintStream 获取原始流。
     */
    bool readFootprintObjects(int componentIndex, QVector<PrimitiveRecord>* objects) const;
    /** @brief 按封装名称扫描已知图元对象 */
    bool readFootprintObjects(const QString& componentName, QVector<PrimitiveRecord>* objects) const;
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
    mutable QString m_errorMessage;
};

}  // namespace EasyKiConverter
