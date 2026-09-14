#pragma once

#include "core/altium/compound/OLECompoundReader.h"

#include <QMap>
#include <QVector>

namespace EasyKiConverter {

/**
 * @brief Altium SchLib 库级读取器
 * @details 在 OLE 和二进制读取基础上解析 FileHeader、SectionKeys 以及组件 Data 流映射。
 *          当前只读，不执行增量写回。
 */
class AltiumSchLibReader {
public:
    /** @brief SchLib 组件目录项 */
    struct ComponentInfo {
        QString name;
        QString sectionKey;
    };

    /** @brief SchLib Data 中的原始记录块 */
    struct Record {
        quint8 flags = 0;
        QByteArray payload;
        QByteArray encoded;
    };

    /**
     * @brief 打开并解析 SchLib 文件
     * @param filePath 文件路径
     * @return 结构完整且组件映射有效时返回 true
     */
    bool open(const QString& filePath);

    /** @brief 获取组件目录 */
    QVector<ComponentInfo> components() const;
    /** @brief 获取 FileHeader 参数 */
    QMap<QString, QString> headerParameters() const;
    /** @brief 获取指定组件的 Data 流 */
    bool readComponentData(int componentIndex, QByteArray* data) const;
    /** @brief 获取指定组件的 Data 流 */
    bool readComponentData(const QString& componentName, QByteArray* data) const;
    /**
     * @brief 拆分指定组件的 Data 记录
     * @details 不解释记录业务字段，保留每条记录的完整编码，确保未知记录可无损转发。
     */
    bool readComponentRecords(int componentIndex, QVector<Record>* records) const;
    /** @brief 按组件名称拆分 Data 记录 */
    bool readComponentRecords(const QString& componentName, QVector<Record>* records) const;
    /** @brief 获取最近一次错误说明 */
    QString errorString() const;
    /** @brief 获取是否发生读取错误 */
    bool hasError() const;

private:
    bool fail(const QString& message);
    int componentIndex(const QString& componentName) const;

    OLECompoundReader m_oleReader;
    QVector<ComponentInfo> m_components;
    QMap<QString, QString> m_headerParameters;
    QString m_errorMessage;
};

}  // namespace EasyKiConverter
