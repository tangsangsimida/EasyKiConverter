#include "OLECompoundWriter.h"

#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

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

    // 创建根节点并注册到 m_pathToNode，避免 finalize() 中 prepend 导致索引偏移
    StorageNode rootNode;
    rootNode.name = "Root Entry";
    rootNode.fullPath = "";
    rootNode.dirIndex = 0;
    m_nodes.append(rootNode);
    m_pathToNode[""] = 0;

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
bool OLECompoundWriter::writeStream(const QString& storagePath, const QString& streamName, const QByteArray& data) {
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
void OLECompoundWriter::serializeDirectoryEntry(const DirectoryEntry& entry, QByteArray& buffer) const {
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
 * @param parentIndex 父节点的目录索引
 * @param children 子节点的 m_nodes 索引列表（已排序）
 * @return 子树根节点的目录索引
 */
uint32_t OLECompoundWriter::buildChildTree(int parentIndex, const QVector<int>& children) {
    if (children.isEmpty()) {
        return NOSTREAM;
    }

    // 使用二分法构建平衡二叉树
    int mid = children.size() / 2;
    int nodeIdx = children[mid];
    int rootDirIdx = m_nodes[nodeIdx].dirIndex;

    // 递归构建左子树
    QVector<int> leftChildren(children.begin(), children.begin() + mid);
    m_directory[rootDirIdx].leftChild = buildChildTree(rootDirIdx, leftChildren);

    // 递归构建右子树
    QVector<int> rightChildren(children.begin() + mid + 1, children.end());
    m_directory[rootDirIdx].rightChild = buildChildTree(rootDirIdx, rightChildren);

    return static_cast<uint32_t>(rootDirIdx);
}

/**
 * @brief 递归为存储节点及其子存储构建红黑树
 * @param nodeIdx 节点在 m_nodes 中的索引
 */
void OLECompoundWriter::buildSubTree(int nodeIdx) {
    StorageNode& node = m_nodes[nodeIdx];
    if (node.children.isEmpty()) {
        return;
    }

    QVector<int> sorted = node.children;
    std::sort(sorted.begin(), sorted.end(), [this](int a, int b) {
        const QString& nameA = m_nodes[a].name;
        const QString& nameB = m_nodes[b].name;
        if (nameA.size() != nameB.size()) {
            return nameA.size() < nameB.size();
        }
        return nameA.compare(nameB, Qt::CaseInsensitive) < 0;
    });

    m_directory[node.dirIndex].child = buildChildTree(node.dirIndex, sorted);

    // 递归处理子存储节点
    for (int childIdx : node.children) {
        if (m_nodes[childIdx].dirIndex >= 0 &&
            m_directory[m_nodes[childIdx].dirIndex].objectType == ObjectType::Storage) {
            buildSubTree(childIdx);
        }
    }
}

/**
 * @brief 构建红黑树结构
 * @details 为所有存储节点构建子节点的平衡二叉树。
 *          目录条目已在 addStorage() / finalize() 中创建，无需重复创建。
 */
void OLECompoundWriter::buildRedBlackTree() {
    // 根节点已在 create() 中创建，dirIndex=0
    int rootNodeIdx = m_pathToNode.value("", -1);
    if (rootNodeIdx < 0) {
        return;
    }

    buildSubTree(rootNodeIdx);
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

    // 计算 FAT 扇区数（每个扇区 128 个条目）
    uint32_t totalFatSectors = static_cast<uint32_t>(m_fat.size()) / (SECTOR_SIZE / 4);
    header[offset++] = static_cast<char>(totalFatSectors & 0xFF);
    header[offset++] = static_cast<char>((totalFatSectors >> 8) & 0xFF);
    header[offset++] = static_cast<char>((totalFatSectors >> 16) & 0xFF);
    header[offset++] = static_cast<char>((totalFatSectors >> 24) & 0xFF);

    // First directory sector
    // CFB sector 0 starts right after the 512-byte header. FAT occupies sectors
    // 0..totalFatSectors-1, so the directory begins at sector totalFatSectors.
    uint32_t firstDirSector = totalFatSectors;
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
        uint32_t dirSectors =
            (static_cast<uint32_t>(m_directory.size()) * DIR_ENTRY_SIZE + SECTOR_SIZE - 1) / SECTOR_SIZE;
        firstMiniFatSector = totalFatSectors + dirSectors;
    }
    header[offset++] = static_cast<char>(firstMiniFatSector & 0xFF);
    header[offset++] = static_cast<char>((firstMiniFatSector >> 8) & 0xFF);
    header[offset++] = static_cast<char>((firstMiniFatSector >> 16) & 0xFF);
    header[offset++] = static_cast<char>((firstMiniFatSector >> 24) & 0xFF);

    // Total mini FAT sectors
    uint32_t totalMiniFatSectors = static_cast<uint32_t>((m_miniFat.size() * 4 + SECTOR_SIZE - 1) / SECTOR_SIZE);
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
    // The first FAT sector is CFB sector 0, immediately after the header.
    for (uint32_t i = 0; i < 109; ++i) {
        if (i < totalFatSectors) {
            uint32_t sectorNum = i;
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
    // 根节点已在 create() 中创建并注册到 m_pathToNode[""]
    // 此处无需再创建，避免 prepend 导致索引偏移

    // 为所有流创建目录条目并分配数据
    for (const StreamData& stream : m_streams) {
        int dirIndex = createDirectoryEntry(stream.streamName, ObjectType::Stream);
        uint64_t dataSize = static_cast<uint64_t>(stream.data.size());

        if (dataSize < MINI_STREAM_CUTOFF) {
            // 小数据写入 Mini-Stream
            uint32_t miniSectorStart = static_cast<uint32_t>(m_miniStream.size() / MINI_SECTOR_SIZE);
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

        // 创建流节点并添加到正确的父节点
        StorageNode streamNode;
        streamNode.name = stream.streamName;
        streamNode.fullPath =
            stream.storagePath.isEmpty() ? stream.streamName : stream.storagePath + "/" + stream.streamName;
        streamNode.dirIndex = dirIndex;
        int streamNodeIdx = m_nodes.size();
        m_nodes.append(streamNode);

        if (stream.storagePath.isEmpty()) {
            // 根流：添加到 Root Entry 的子列表
            if (m_pathToNode.contains("")) {
                m_nodes[m_pathToNode[""]].children.append(streamNodeIdx);
            }
        } else {
            // 子流：添加到父 Storage 的子列表
            if (m_pathToNode.contains(stream.storagePath)) {
                m_nodes[m_pathToNode[stream.storagePath]].children.append(streamNodeIdx);
            }
        }
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
        uint32_t miniStreamSectors = (static_cast<uint32_t>(m_miniStream.size()) + SECTOR_SIZE - 1) / SECTOR_SIZE;
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

    // 确保目录大小是扇区对齐的
    uint32_t dirSectors = (static_cast<uint32_t>(m_directory.size()) * DIR_ENTRY_SIZE + SECTOR_SIZE - 1) / SECTOR_SIZE;
    while (m_directory.size() < static_cast<int>(dirSectors * (SECTOR_SIZE / DIR_ENTRY_SIZE))) {
        DirectoryEntry empty;
        empty.objectType = ObjectType::Unknown;
        empty.startSector = FREESECT;
        m_directory.append(empty);
    }

    // 确保 Mini-FAT 大小是扇区对齐的
    while (!m_miniFat.isEmpty() && m_miniFat.size() % (SECTOR_SIZE / 4) != 0) {
        m_miniFat.append(FREESECT);
    }
    uint32_t miniFatSectors = static_cast<uint32_t>((m_miniFat.size() * 4 + SECTOR_SIZE - 1) / SECTOR_SIZE);

    // ---- 构建完整的 FAT（包含所有扇区的条目）----
    // 文件布局：FAT(0..N-1) | dir(N..N+D-1) | MiniFAT | data
    // m_fat 目前只有数据扇区的链式条目（每个条目=一个数据扇区的后继指针）
    // 需要补充 FAT/dir/MiniFAT 的条目，并为未使用的数据扇区填 FREESECT
    uint32_t dataSectorCount = static_cast<uint32_t>(m_fat.size());
    uint32_t entriesPerSector = SECTOR_SIZE / 4;

    // 迭代计算 FAT 扇区数（FAT 扇区自身也占条目）
    uint32_t fatSectors = 0;
    uint32_t totalEntries = 0;
    for (int iter = 0; iter < 10; ++iter) {
        uint32_t newTotal = fatSectors + dirSectors + miniFatSectors + dataSectorCount;
        newTotal = ((newTotal + entriesPerSector - 1) / entriesPerSector) * entriesPerSector;
        uint32_t newFatSectors = newTotal / entriesPerSector;
        if (newFatSectors == fatSectors && newTotal == totalEntries) {
            break;
        }
        fatSectors = newFatSectors;
        totalEntries = newTotal;
    }

    // 构建完整的 FAT 向量
    QVector<uint32_t> fullFat;
    fullFat.reserve(totalEntries);

    // [0..fatSectors-1] FAT sectors → 标记为 FATSECT
    for (uint32_t i = 0; i < fatSectors; ++i) {
        fullFat.append(FATSECT);
    }

    // [fatSectors .. fatSectors+dirSectors-1] Directory sectors → 链式
    for (uint32_t i = 0; i < dirSectors; ++i) {
        if (i < dirSectors - 1) {
            fullFat.append(fatSectors + i + 1);
        } else {
            fullFat.append(ENDOFCHAIN);
        }
    }

    // MiniFAT sectors（通过 header 引用，FAT 中标记为普通已分配扇区）
    for (uint32_t i = 0; i < miniFatSectors; ++i) {
        fullFat.append(ENDOFCHAIN);
    }

    // Data sectors → 来自 m_fat，需要加偏移量
    // m_fat 中的链式指针是从 0 开始的数据扇区索引，需要偏移到文件中的实际位置
    uint32_t dataOffset = fatSectors + dirSectors + miniFatSectors;  // 不含 dataSectorCount
    for (uint32_t val : m_fat) {
        if (val == ENDOFCHAIN) {
            fullFat.append(ENDOFCHAIN);
        } else {
            fullFat.append(val + dataOffset);
        }
    }

    // 填充 FREESECT 到 totalEntries（未使用的数据扇区 + FAT 对齐）
    while (static_cast<uint32_t>(fullFat.size()) < totalEntries) {
        fullFat.append(FREESECT);
    }

    m_fat = fullFat;

    // Directory stream start sectors must be absolute CFB sector numbers.
    // Mini-stream entries keep their offset inside the root mini stream.
    for (DirectoryEntry& entry : m_directory) {
        if (entry.objectType == ObjectType::Root && entry.streamSize > 0) {
            entry.startSector += dataOffset;
        } else if (entry.objectType == ObjectType::Stream && entry.streamSize >= MINI_STREAM_CUTOFF &&
                   entry.startSector != ENDOFCHAIN) {
            entry.startSector += dataOffset;
        }
    }
}

/**
 * @brief 保存到文件
 */
bool OLECompoundWriter::saveToFile(const QString& filePath) {
    if (!m_initialized) {
        qWarning() << "OLECompoundWriter::saveToFile: Not initialized";
        return false;
    }

    finalize();

    qDebug() << "OLECompoundWriter::saveToFile: Writing to" << filePath << "streams:" << m_streams.size()
             << "directory:" << m_directory.size() << "fat:" << m_fat.size();

    // 确保父目录存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.dir().exists()) {
        qDebug() << "OLECompoundWriter::saveToFile: Creating parent directory:" << fileInfo.path();
        QDir().mkpath(fileInfo.path());
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "OLECompoundWriter::saveToFile: Failed to open file:" << filePath
                   << "error:" << file.errorString() << "exists:" << QFile::exists(filePath)
                   << "isDir:" << QFileInfo(filePath).isDir();
        return false;
    }

    // 写入头部（512 字节）
    QByteArray header;
    serializeFileHeader(header);
    file.write(header);

    // 写入 FAT 扇区（每个扇区 128 个条目）
    uint32_t fatSectorCount = static_cast<uint32_t>(m_fat.size()) / (SECTOR_SIZE / 4);
    uint32_t entriesPerSector = SECTOR_SIZE / 4;
    for (uint32_t i = 0; i < fatSectorCount; ++i) {
        QByteArray sectorData(SECTOR_SIZE, 0);
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
