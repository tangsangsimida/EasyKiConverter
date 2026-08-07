#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>

namespace EasyKiConverter {

/**
 * @brief OLE 复合文档 V3 写入器
 * @details 仅支持写入，不支持读取。实现 OLE Structured Storage V3 格式（512 字节扇区）。
 *          用于生成 Altium Designer 可读取的 .SchLib / .PcbLib 文件。
 *
 * 参考：OpenMcdf (https://github.com/ironfede/openmcdf)
 * 规范：[MS-CFB] Compound File Binary File Format
 */
class OLECompoundWriter {
public:
    OLECompoundWriter();
    ~OLECompoundWriter();

    /**
     * @brief 创建新的 OLE 复合文档
     * @return 是否成功初始化
     */
    bool create();

    /**
     * @brief 保存到文件
     * @param filePath 输出文件路径
     * @return 是否成功
     */
    bool saveToFile(const QString& filePath);

    /**
     * @brief 在指定存储下创建子存储
     * @param parentPath 父存储路径（空字符串表示根存储）
     * @param name 存储名称
     * @return 是否成功
     */
    bool addStorage(const QString& parentPath, const QString& name);

    /**
     * @brief 在根存储下创建子存储
     * @param name 存储名称
     * @return 是否成功
     */
    bool addStorage(const QString& name);

    /**
     * @brief 在指定存储下写入流数据
     * @param storagePath 存储路径（空字符串表示根存储）
     * @param streamName 流名称
     * @param data 流数据
     * @return 是否成功
     */
    bool writeStream(const QString& storagePath, const QString& streamName, const QByteArray& data);

    /**
     * @brief 在根存储下写入流数据
     * @param name 流名称
     * @param data 流数据
     * @return 是否成功
     */
    bool writeStream(const QString& name, const QByteArray& data);

private:
    /** @brief OLE 常量 */
    static constexpr uint32_t ENDOFCHAIN = 0xFFFFFFFE;
    static constexpr uint32_t FREESECT = 0xFFFFFFFF;
    static constexpr uint32_t FATSECT = 0xFFFFFFFD;
    static constexpr uint32_t NOSTREAM = 0xFFFFFFFF;
    static constexpr uint32_t SECTOR_SIZE = 512;
    static constexpr uint32_t MINI_SECTOR_SIZE = 64;
    static constexpr uint32_t MINI_STREAM_CUTOFF = 4096;
    static constexpr uint32_t DIR_ENTRY_SIZE = 128;

    /** @brief 目录条目对象类型 */
    enum class ObjectType : uint8_t { Unknown = 0, Storage = 1, Stream = 2, Root = 5 };

    /** @brief 目录条目结构（128 字节） */
    struct DirectoryEntry {
        uint16_t name[32] = {0};  ///< UTF-16LE 名称（最大 31 字符 + null）
        uint16_t nameSize = 0;  ///< 名称字节数（含 null 终止符）
        ObjectType objectType = ObjectType::Unknown;
        uint8_t colorFlag = 1;  ///< 0=Red, 1=Black（红黑树节点颜色）
        uint32_t leftChild = NOSTREAM;
        uint32_t rightChild = NOSTREAM;
        uint32_t child = NOSTREAM;
        uint8_t clsid[16] = {0};
        uint32_t stateBits = 0;
        uint64_t creationTime = 0;
        uint64_t modifiedTime = 0;
        uint32_t startSector = ENDOFCHAIN;
        uint64_t streamSize = 0;
    };

    /**
     * @brief 内部节点结构，用于跟踪存储树
     */
    struct StorageNode {
        QString name;  ///< 节点名称
        QString fullPath;  ///< 完整路径
        int dirIndex = -1;  ///< 目录条目索引
        QVector<int> children;  ///< 子节点索引列表
    };

    // ---- 目录操作 ----
    int createDirectoryEntry(const QString& name, ObjectType type);
    void buildRedBlackTree();
    void buildSubTree(int nodeIdx);
    uint32_t buildChildTree(int parentIndex, const QVector<int>& children);

    // ---- 数据写入 ----
    void serializeDirectoryEntry(const DirectoryEntry& entry, QByteArray& buffer) const;
    void serializeFileHeader(QByteArray& header) const;
    void finalize();

    // ---- 内部状态 ----
    bool m_initialized = false;
    QVector<DirectoryEntry> m_directory;  ///< 目录条目列表
    QVector<StorageNode> m_nodes;  ///< 存储节点树
    QHash<QString, int> m_pathToNode;  ///< 路径 → 节点索引映射

    // 流数据（按存储路径+流名索引）
    struct StreamData {
        QString storagePath;
        QString streamName;
        QByteArray data;
    };

    QVector<StreamData> m_streams;

    // 扇区数据
    QVector<QByteArray> m_dataSectors;  ///< 普通数据扇区
    QByteArray m_miniStream;  ///< Mini-Stream 数据
    QVector<uint32_t> m_fat;  ///< FAT 表
    QVector<uint32_t> m_miniFat;  ///< Mini-FAT 表
};

}  // namespace EasyKiConverter
