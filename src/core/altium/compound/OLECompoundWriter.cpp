#include "OLECompoundWriter.h"

#include <QDataStream>
#include <QFile>

namespace EasyKiConverter {

/**
 * @brief 构造函数
 */
OLECompoundWriter::OLECompoundWriter() = default;

/**
 * @brief 析构函数
 */
OLECompoundWriter::~OLECompoundWriter() = default;

/**
 * @brief 创建新的 OLE 复合文档
 */
bool OLECompoundWriter::create() {
    m_initialized = false;
    m_directory.clear();
    m_nodes.clear();
    m_pathToNode.clear();
    m_streams.clear();
    m_dataSectors.clear();
    m_miniStream.clear();
    m_fat.clear();
    m_miniFat.clear();

    // 创建 Root Entry 目录条目（索引 0）
    int rootIdx = createDirectoryEntry("Root Entry", ObjectType::Root);
    if (rootIdx != 0) {
        return false;
    }

    m_initialized = true;
    return true;
}

/**
 * @brief 创建目录条目
 */
int OLECompoundWriter::createDirectoryEntry(const QString& name, ObjectType type) {
    DirectoryEntry entry;
    entry.objectType = type;

    // 写入 UTF-16LE 名称
    QString utf16Name = name;
    int charCount = qMin(utf16Name.length(), 31);
    for (int i = 0; i < charCount; ++i) {
        entry.name[i] = static_cast<uint16_t>(utf16Name.at(i).unicode());
    }
    entry.nameSize = static_cast<uint16_t>((charCount + 1) * 2);  // 含 null 终止符

    int index = m_directory.size();
    m_directory.append(entry);

    return index;
}

/**
 * @brief 在指定存储下创建子存储
 */
bool OLECompoundWriter::addStorage(const QString& parentPath, const QString& name) {
    if (!m_initialized) {
        return false;
    }

    QString fullPath = parentPath.isEmpty() ? name : parentPath + "/" + name;

    // 检查是否已存在
    if (m_pathToNode.contains(fullPath)) {
        return true;  // 已存在，跳过
    }

    int dirIndex = createDirectoryEntry(name, ObjectType::Storage);

    // 创建节点
    StorageNode node;
    node.name = name;
    node.fullPath = fullPath;
    node.dirIndex = dirIndex;
    int nodeIndex = m_nodes.size();
    m_nodes.append(node);
    m_pathToNode[fullPath] = nodeIndex;

    // 添加到父节点的子列表
    if (parentPath.isEmpty()) {
        // 根存储的子节点
        if (m_pathToNode.contains("")) {
            m_nodes[m_pathToNode[""]].children.append(nodeIndex);
        }
    } else {
        if (m_pathToNode.contains(parentPath)) {
            m_nodes[m_pathToNode[parentPath]].children.append(nodeIndex);
        }
    }

    return true;
}

/**
 * @brief 在根存储下创建子存储
 */
bool OLECompoundWriter::addStorage(const QString& name) {
    return addStorage("", name);
}

/**
 * @brief 在指定存储下写入流数据
 */
bool OLECompoundWriter::writeStream(const QString& storagePath, const QString& streamName,
                                    const QByteArray& data) {
    if (!m_initialized) {
        return false;
    }

    StreamData stream;
    stream.storagePath = storagePath;
    stream.streamName = streamName;
    stream.data = data;
    m_streams.append(stream);

    return true;
}

/**
 * @brief 在根存储下写入流数据
 */
bool OLECompoundWriter::writeStream(const QString& name, const QByteArray& data) {
    return writeStream("", name, data);
}

/**
 * @brief 将目录条目序列化为 128 字节
 */
void OLECompoundWriter::serializeDirectoryEntry(const DirectoryEntry& entry,
                                                 QByteArray& buffer) const {
    buffer.resize(DIR_ENTRY_SIZE);
    buffer.fill(0);
    int offset = 0;

    // 名称（64 字节，UTF-16LE）
    for (int i = 0; i < 32; ++i) {
        buffer[offset++] = static_cast<char>(entry.name[i] & 0xFF);
        buffer[offset++] = static_cast<char>((entry.name[i] >> 8) & 0xFF);
    }

    // 名称大小（2 字节）
    buffer[offset++] = static_cast<char>(entry.nameSize & 0xFF);
    buffer[offset++] = static_cast<char>((entry.nameSize >> 8) & 0xFF);

    // 对象类型（1 字节）
    buffer[offset++] = static_cast<char>(entry.objectType);

    // 颜色标志（1 字节）
    buffer[offset++] = static_cast<char>(entry.colorFlag);

    // 左子节点（4 字节）
    buffer[offset++] = static_cast<char>(entry.leftChild & 0xFF);
    buffer[offset++] = static_cast<char>((entry.leftChild >> 8) & 0xFF);
    buffer[offset++] = static_cast<char>((entry.leftChild >> 16) & 0xFF);
    buffer[offset++] = static_cast<char>((entry.leftChild >> 24) & 0xFF);

    // 右子节点（4 字节）
    buffer[offset++] = static_cast<char>(entry.rightChild & 0xFF);
    buffer[offset++] = static_cast<char>((entry.rightChild >> 8) & 0xFF);
    buffer[offset++] = static_cast<char>((entry.rightChild >> 16) & 0xFF);
    buffer[offset++] = static_cast<char>((entry.rightChild >> 24) & 0xFF);

    // 子节点（4 字节）
    buffer[offset++] = static_cast<char>(entry.child & 0xFF);
    buffer[offset++] = static_cast<char>((entry.child >> 8) & 0xFF);
    buffer[offset++] = static_cast<char>((entry.child >> 16) & 0xFF);
    buffer[offset++] = static_cast<char>((entry.child >> 24) & 0xFF);

    // CLSID（16 字节）
    for (int i = 0; i < 16; ++i) {
        buffer[offset++] = static_cast<char>(entry.clsid[i]);
    }

    // StateBits（4 字节）
    buffer[offset++] = static_cast<char>(entry.stateBits & 0xFF);
    buffer[offset++] = static_cast<char>((entry.stateBits >> 8) & 0xFF);
    buffer[offset++] = static_cast<char>((entry.stateBits >> 16) & 0xFF);
    buffer[offset++] = static_cast<char>((entry.stateBits >> 24) & 0xFF);

    // 创建时间（8 字节）
    for (int i = 0; i < 8; ++i) {
        buffer[offset++] = static_cast<char>((entry.creationTime >> (i * 8)) & 0xFF);
    }

    // 修改时间（8 字节）
    for (int i = 0; i < 8; ++i) {
        buffer[offset++] = static_cast<char>((entry.modifiedTime >> (i * 8)) & 0xFF);
    }

    // 起始扇区（4 字节）
    buffer[offset++] = static_cast<char>(entry.startSector & 0xFF);
    buffer[offset++] = static_cast<char>((entry.startSector >> 8) & 0xFF);
    buffer[offset++] = static_cast<char>((entry.startSector >> 16) & 0xFF);
    buffer[offset++] = static_cast<char>((entry.startSector >> 24) & 0xFF);

    // 流大小（8 字节）
    for (int i = 0; i < 8; ++i) {
        buffer[offset++] = static_cast<char>((entry.streamSize >> (i * 8)) & 0xFF);
    }
}

/**
 * @brief 使用递归构建红黑树子树
 */
uint32_t OLECompoundWriter::buildChildTree(int parentIndex, const QVector<int>& children) {
    if (children.isEmpty()) {
        return NOSTREAM;
    }

    // 使用二分法构建平衡二叉树
    int mid = children.size() / 2;
    int rootDirIdx = children[mid];

    // 递归构建左子树
    QVector<int> leftChildren(children.begin(), children.begin() + mid);
    m_directory[rootDirIdx].leftChild = buildChildTree(rootDirIdx, leftChildren);

    // 递归构建右子树
    QVector<int> rightChildren(children.begin() + mid + 1, children.end());
    m_directory[rootDirIdx].rightChild = buildChildTree(rootDirIdx, rightChildren);

    return static_cast<uint32_t>(rootDirIdx);
}

/**
 * @brief 构建红黑树结构
 */
void OLECompoundWriter::buildRedBlackTree() {
    // 根节点是目录条目 0
    int rootNodeIdx = m_pathToNode.value("", -1);
    if (rootNodeIdx < 0) {
        return;
    }

    StorageNode& rootNode = m_nodes[rootNodeIdx];
    rootNode.dirIndex = 0;

    // 排序子节点
    QVector<int> sortedChildren = rootNode.children;
    std::sort(sortedChildren.begin(), sortedChildren.end(),
              [this](int a, int b) { return m_nodes[a].name < m_nodes[b].name; });

    // 设置 Root Entry 的子节点
    m_directory[0].child = buildChildTree(0, sortedChildren);

    // 为所有存储节点递归设置子节点
    for (int nodeIdx : rootNode.children) {
        StorageNode& node = m_nodes[nodeIdx];
        if (!node.children.isEmpty()) {
            // 更新目录条目索引
            for (int& childNodeIdx : node.children) {
                m_nodes[childNodeIdx].dirIndex = createDirectoryEntry(
                    m_nodes[childNodeIdx].name, ObjectType::Storage);
            }

            QVector<int> sorted = node.children;
            std::sort(sorted.begin(), sorted.end(),
                      [this](int a, int b) { return m_nodes[a].name < m_nodes[b].name; });

            m_directory[node.dirIndex].child = buildChildTree(node.dirIndex, sorted);
        }
    }
}

/**
 * @brief 序列化文件头部（512 字节）
 */
void OLECompoundWriter::serializeFileHeader(QByteArray& header) const {
    header.resize(SECTOR_SIZE);
    header.fill(0);
    int offset = 0;

    // Magic number (8 bytes)
    header[offset++] = static_cast<char>(0xD0);
    header[offset++] = static_cast<char>(0xCF);
    header[offset++] = static_cast<char>(0x11);
    header[offset++] = static_cast<char>(0xE0);
    header[offset++] = static_cast<char>(0xA1);
    header[offset++] = static_cast<char>(0xB1);
    header[offset++] = static_cast<char>(0x1A);
    header[offset++] = static_cast<char>(0xE1);

    // CLSID (16 bytes, all zeros)
    offset += 16;

    // Minor version (2 bytes) = 0x003E
    header[offset++] = 0x3E;
    header[offset++] = 0x00;

    // Major version (2 bytes) = 0x0003 (V3)
    header[offset++] = 0x03;
    header[offset++] = 0x00;

    // Byte order (2 bytes) = 0xFFFE (little-endian)
    header[offset++] = static_cast<char>(0xFE);
    header[offset++] = static_cast<char>(0xFF);

    // Sector shift (2 bytes) = 9 (512 bytes)
    header[offset++] = 0x09;
    header[offset++] = 0x00;

    // Mini sector shift (2 bytes) = 6 (64 bytes)
    header[offset++] = 0x06;
    header[offset++] = 0x00;

    // Reserved (6 bytes)
    offset += 6;

    // Total directory sectors (4 bytes) = 0 for V3
    offset += 4;

    // 计算 FAT 扇区数
    uint32_t totalFatSectors = static_cast<uint32_t>(m_fat.size());
    header[offset++] = static_cast<char>(totalFatSectors & 0xFF);
    header[offset++] = static_cast<char>((totalFatSectors >> 8) & 0xFF);
    header[offset++] = static_cast<char>((totalFatSectors >> 16) & 0xFF);
    header[offset++] = static_cast<char>((totalFatSectors >> 24) & 0xFF);

    // First directory sector
    // 目录紧跟在 FAT 扇区之后
    uint32_t firstDirSector = totalFatSectors + 1;  // +1 for header sector
    header[offset++] = static_cast<char>(firstDirSector & 0xFF);
    header[offset++] = static_cast<char>((firstDirSector >> 8) & 0xFF);
    header[offset++] = static_cast<char>((firstDirSector >> 16) & 0xFF);
    header[offset++] = static_cast<char>((firstDirSector >> 24) & 0xFF);

    // Transaction signature (4 bytes)
    offset += 4;

    // Mini stream cutoff (4 bytes) = 4096
    header[offset++] = 0x00;
    header[offset++] = 0x10;
    header[offset++] = 0x00;
    header[offset++] = 0x00;

    // First mini FAT sector
    uint32_t firstMiniFatSector = ENDOFCHAIN;
    if (!m_miniFat.isEmpty()) {
        // Mini FAT 在目录之后
        uint32_t dirSectors = (static_cast<uint32_t>(m_directory.size()) * DIR_ENTRY_SIZE
                               + SECTOR_SIZE - 1) / SECTOR_SIZE;
        firstMiniFatSector = totalFatSectors + 1 + dirSectors;
    }
    header[offset++] = static_cast<char>(firstMiniFatSector & 0xFF);
    header[offset++] = static_cast<char>((firstMiniFatSector >> 8) & 0xFF);
    header[offset++] = static_cast<char>((firstMiniFatSector >> 16) & 0xFF);
    header[offset++] = static_cast<char>((firstMiniFatSector >> 24) & 0xFF);

    // Total mini FAT sectors
    uint32_t totalMiniFatSectors = static_cast<uint32_t>(
        (m_miniFat.size() * 4 + SECTOR_SIZE - 1) / SECTOR_SIZE);
    header[offset++] = static_cast<char>(totalMiniFatSectors & 0xFF);
    header[offset++] = static_cast<char>((totalMiniFatSectors >> 8) & 0xFF);
    header[offset++] = static_cast<char>((totalMiniFatSectors >> 16) & 0xFF);
    header[offset++] = static_cast<char>((totalMiniFatSectors >> 24) & 0xFF);

    // First DIFAT sector
    uint32_t firstDifatSector = ENDOFCHAIN;
    header[offset++] = static_cast<char>(firstDifatSector & 0xFF);
    header[offset++] = static_cast<char>((firstDifatSector >> 8) & 0xFF);
    header[offset++] = static_cast<char>((firstDifatSector >> 16) & 0xFF);
    header[offset++] = static_cast<char>((firstDifatSector >> 24) & 0xFF);

    // Total DIFAT sectors
    offset += 4;

    // DIFAT array (109 entries)
    // 前 totalFatSectors 个 FAT 扇区号
    for (uint32_t i = 0; i < 109; ++i) {
        if (i < totalFatSectors) {
            uint32_t sectorNum = i + 1;  // FAT 扇区从 1 开始（0 是头部）
            header[offset++] = static_cast<char>(sectorNum & 0xFF);
            header[offset++] = static_cast<char>((sectorNum >> 8) & 0xFF);
            header[offset++] = static_cast<char>((sectorNum >> 16) & 0xFF);
            header[offset++] = static_cast<char>((sectorNum >> 24) & 0xFF);
        } else {
            header[offset++] = static_cast<char>(FREESECT & 0xFF);
            header[offset++] = static_cast<char>((FREESECT >> 8) & 0xFF);
            header[offset++] = static_cast<char>((FREESECT >> 16) & 0xFF);
            header[offset++] = static_cast<char>((FREESECT >> 24) & 0xFF);
        }
    }
}

/**
 * @brief 最终处理：分配扇区、构建目录、准备序列化数据
 */
void OLECompoundWriter::finalize() {
    // 确保根节点存在
    if (!m_pathToNode.contains("")) {
        StorageNode rootNode;
        rootNode.name = "Root Entry";
        rootNode.fullPath = "";
        rootNode.dirIndex = 0;
        m_nodes.prepend(rootNode);
        m_pathToNode[""] = 0;
    }

    // 为所有流创建目录条目并分配数据
    for (const StreamData& stream : m_streams) {
        int dirIndex = createDirectoryEntry(stream.streamName, ObjectType::Stream);
        uint64_t dataSize = static_cast<uint64_t>(stream.data.size());

        if (dataSize < MINI_STREAM_CUTOFF) {
            // 小数据写入 Mini-Stream
            uint32_t miniSectorStart = static_cast<uint32_t>(
                m_miniStream.size() / MINI_SECTOR_SIZE);
            m_miniStream.append(stream.data);
            // 对齐到 mini 扇区边界
            int remainder = stream.data.size() % MINI_SECTOR_SIZE;
            if (remainder > 0) {
                m_miniStream.append(QByteArray(MINI_SECTOR_SIZE - remainder, 0));
            }
            m_directory[dirIndex].startSector = miniSectorStart;
            m_directory[dirIndex].streamSize = dataSize;

            // 更新 Mini-FAT
            uint32_t miniSectorsNeeded = (dataSize + MINI_SECTOR_SIZE - 1) / MINI_SECTOR_SIZE;
            for (uint32_t i = 0; i < miniSectorsNeeded; ++i) {
                if (i < miniSectorsNeeded - 1) {
                    m_miniFat.append(miniSectorStart + i + 1);
                } else {
                    m_miniFat.append(ENDOFCHAIN);
                }
            }
        } else {
            // 大数据写入普通扇区
            uint32_t sectorStart = static_cast<uint32_t>(m_dataSectors.size());
            int remaining = stream.data.size();
            int srcOffset = 0;
            while (remaining > 0) {
                QByteArray sectorData(SECTOR_SIZE, 0);
                int chunkSize = qMin(remaining, static_cast<int>(SECTOR_SIZE));
                memcpy(sectorData.data(), stream.data.constData() + srcOffset, chunkSize);
                m_dataSectors.append(sectorData);
                srcOffset += chunkSize;
                remaining -= chunkSize;
            }
            m_directory[dirIndex].startSector = sectorStart;
            m_directory[dirIndex].streamSize = dataSize;

            // 更新 FAT
            uint32_t sectorsNeeded = (dataSize + SECTOR_SIZE - 1) / SECTOR_SIZE;
            for (uint32_t i = 0; i < sectorsNeeded; ++i) {
                if (i < sectorsNeeded - 1) {
                    m_fat.append(sectorStart + i + 1);
                } else {
                    m_fat.append(ENDOFCHAIN);
                }
            }
        }

        // 将流添加到父存储的子列表
        QString parentPath = stream.storagePath;
        if (parentPath.isEmpty()) {
            if (m_pathToNode.contains("")) {
                m_nodes[m_pathToNode[""]].children.append(m_nodes.size() - 1);
            }
        }
        // 注意：流的目录条目已在 createDirectoryEntry 中创建
        // 但节点还未添加到 m_nodes，这里补充
        StorageNode streamNode;
        streamNode.name = stream.streamName;
        streamNode.fullPath = parentPath.isEmpty()
                                  ? stream.streamName
                                  : parentPath + "/" + stream.streamName;
        streamNode.dirIndex = dirIndex;
        m_nodes.append(streamNode);
    }

    // 为 Root Entry 分配 Mini-Stream 的存储扇区
    if (!m_miniStream.isEmpty()) {
        uint32_t miniStreamSectorStart = static_cast<uint32_t>(m_dataSectors.size());
        int remaining = m_miniStream.size();
        int srcOffset = 0;
        while (remaining > 0) {
            QByteArray sectorData(SECTOR_SIZE, 0);
            int chunkSize = qMin(remaining, static_cast<int>(SECTOR_SIZE));
            memcpy(sectorData.data(), m_miniStream.constData() + srcOffset, chunkSize);
            m_dataSectors.append(sectorData);
            srcOffset += chunkSize;
            remaining -= chunkSize;
        }
        m_directory[0].startSector = miniStreamSectorStart;
        m_directory[0].streamSize = static_cast<uint64_t>(m_miniStream.size());

        // 更新 FAT
        uint32_t miniStreamSectors = (static_cast<uint32_t>(m_miniStream.size())
                                       + SECTOR_SIZE - 1) / SECTOR_SIZE;
        for (uint32_t i = 0; i < miniStreamSectors; ++i) {
            if (i < miniStreamSectors - 1) {
                m_fat.append(miniStreamSectorStart + i + 1);
            } else {
                m_fat.append(ENDOFCHAIN);
            }
        }
    }

    // 构建红黑树
    buildRedBlackTree();

    // 确保 FAT 大小是扇区对齐的
    while (m_fat.size() % (SECTOR_SIZE / 4) != 0) {
        m_fat.append(FREESECT);
    }

    // 确保 Mini-FAT 大小是扇区对齐的
    while (!m_miniFat.isEmpty() && m_miniFat.size() % (SECTOR_SIZE / 4) != 0) {
        m_miniFat.append(FREESECT);
    }

    // 确保目录大小是扇区对齐的
    uint32_t dirSectors = (static_cast<uint32_t>(m_directory.size()) * DIR_ENTRY_SIZE
                            + SECTOR_SIZE - 1) / SECTOR_SIZE;
    while (m_directory.size() < static_cast<int>(dirSectors * (SECTOR_SIZE / DIR_ENTRY_SIZE))) {
        DirectoryEntry empty;
        empty.objectType = ObjectType::Unknown;
        empty.startSector = FREESECT;
        m_directory.append(empty);
    }
}

/**
 * @brief 保存到文件
 */
bool OLECompoundWriter::saveToFile(const QString& filePath) {
    if (!m_initialized) {
        return false;
    }

    finalize();

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    // 写入头部（512 字节）
    QByteArray header;
    serializeFileHeader(header);
    file.write(header);

    // 写入 FAT 扇区
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_fat.size()); ++i) {
        QByteArray sectorData(SECTOR_SIZE, 0);
        uint32_t entriesPerSector = SECTOR_SIZE / 4;
        uint32_t startIdx = i * entriesPerSector;
        for (uint32_t j = 0; j < entriesPerSector && (startIdx + j) < static_cast<uint32_t>(m_fat.size()); ++j) {
            uint32_t val = m_fat[startIdx + j];
            int off = j * 4;
            sectorData[off] = static_cast<char>(val & 0xFF);
            sectorData[off + 1] = static_cast<char>((val >> 8) & 0xFF);
            sectorData[off + 2] = static_cast<char>((val >> 16) & 0xFF);
            sectorData[off + 3] = static_cast<char>((val >> 24) & 0xFF);
        }
        file.write(sectorData);
    }

    // 写入目录扇区
    QByteArray dirData;
    for (const DirectoryEntry& entry : m_directory) {
        QByteArray entryData;
        serializeDirectoryEntry(entry, entryData);
        dirData.append(entryData);
    }
    file.write(dirData);

    // 写入 Mini-FAT 扇区
    if (!m_miniFat.isEmpty()) {
        QByteArray miniFatData;
        for (uint32_t val : m_miniFat) {
            miniFatData.append(static_cast<char>(val & 0xFF));
            miniFatData.append(static_cast<char>((val >> 8) & 0xFF));
            miniFatData.append(static_cast<char>((val >> 16) & 0xFF));
            miniFatData.append(static_cast<char>((val >> 24) & 0xFF));
        }
        file.write(miniFatData);
    }

    // 写入数据扇区
    for (const QByteArray& sector : m_dataSectors) {
        file.write(sector);
    }

    file.close();
    return true;
}

// ---- 序列化辅助方法 ----

}  // namespace EasyKiConverter
