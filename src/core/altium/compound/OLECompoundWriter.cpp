#include "OLECompoundWriter.h"

#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

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
    m_finalized = false;
    m_directory.clear();
    m_nodes.clear();
    m_pathToNode.clear();
    m_entryPaths.clear();
    m_streams.clear();
    m_dataSectors.clear();
    m_difatSectors.clear();
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
    if (!m_initialized || m_finalized || !isValidEntryName(name) || !m_pathToNode.contains(parentPath)) {
        return false;
    }

    QString fullPath = parentPath.isEmpty() ? name : parentPath + "/" + name;

    const QString foldedPath = fullPath.toCaseFolded();
    if (m_entryPaths.contains(foldedPath)) {
        return false;
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
    m_entryPaths.insert(foldedPath);

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
    if (!m_initialized || m_finalized || !isValidEntryName(streamName) || !m_pathToNode.contains(storagePath)) {
        return false;
    }

    const QString fullPath = storagePath.isEmpty() ? streamName : storagePath + "/" + streamName;
    const QString foldedPath = fullPath.toCaseFolded();
    if (m_entryPaths.contains(foldedPath)) {
        return false;
    }

    StreamData stream;
    stream.storagePath = storagePath;
    stream.streamName = streamName;
    stream.data = data;
    m_streams.append(stream);
    m_entryPaths.insert(foldedPath);

    return true;
}

bool OLECompoundWriter::isValidEntryName(const QString& name) const {
    return !name.isEmpty() && name.size() <= 31 && !name.contains('/') && !name.contains('\\') && !name.contains(':') &&
           !name.contains('!') && !name.contains(QChar::Null);
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
uint32_t OLECompoundWriter::buildChildTree(const QVector<int>& children) {
    if (children.isEmpty()) {
        return NOSTREAM;
    }

    // CFB 的 sibling tree 不只是二叉搜索树，还必须满足红黑树不变量。
    // children 已按 CFB 名称顺序排列；逐项插入并进行标准红黑树修复，
    // 可以同时保持目录顺序和所有根到叶路径相同的黑高。
    QVector<int> parent(m_directory.size(), -1);
    int root = -1;

    auto left = [this](int index) {
        const uint32_t child = m_directory[index].leftChild;
        return child == NOSTREAM ? -1 : static_cast<int>(child);
    };
    auto right = [this](int index) {
        const uint32_t child = m_directory[index].rightChild;
        return child == NOSTREAM ? -1 : static_cast<int>(child);
    };
    auto setLeft = [this, &parent](int index, int child) {
        m_directory[index].leftChild = child < 0 ? NOSTREAM : static_cast<uint32_t>(child);
        if (child >= 0) {
            parent[child] = index;
        }
    };
    auto setRight = [this, &parent](int index, int child) {
        m_directory[index].rightChild = child < 0 ? NOSTREAM : static_cast<uint32_t>(child);
        if (child >= 0) {
            parent[child] = index;
        }
    };

    auto rotateLeft = [&](int pivot) {
        const int promoted = right(pivot);
        const int oldParent = parent[pivot];
        setRight(pivot, left(promoted));
        parent[promoted] = oldParent;
        if (oldParent < 0) {
            root = promoted;
        } else if (left(oldParent) == pivot) {
            m_directory[oldParent].leftChild = static_cast<uint32_t>(promoted);
        } else {
            m_directory[oldParent].rightChild = static_cast<uint32_t>(promoted);
        }
        setLeft(promoted, pivot);
    };
    auto rotateRight = [&](int pivot) {
        const int promoted = left(pivot);
        const int oldParent = parent[pivot];
        setLeft(pivot, right(promoted));
        parent[promoted] = oldParent;
        if (oldParent < 0) {
            root = promoted;
        } else if (left(oldParent) == pivot) {
            m_directory[oldParent].leftChild = static_cast<uint32_t>(promoted);
        } else {
            m_directory[oldParent].rightChild = static_cast<uint32_t>(promoted);
        }
        setRight(promoted, pivot);
    };

    for (int nodeIndex : children) {
        const int inserted = m_nodes[nodeIndex].dirIndex;
        m_directory[inserted].leftChild = NOSTREAM;
        m_directory[inserted].rightChild = NOSTREAM;
        m_directory[inserted].colorFlag = 0;  // 新节点为红色

        // 排序后的节点总是插入当前最大值的右侧。旋转只改变树形，
        // 不改变中序顺序，因此该性质在后续插入中仍成立。
        int insertionParent = -1;
        int cursor = root;
        while (cursor >= 0) {
            insertionParent = cursor;
            cursor = right(cursor);
        }
        parent[inserted] = insertionParent;
        if (insertionParent < 0) {
            root = inserted;
        } else {
            m_directory[insertionParent].rightChild = static_cast<uint32_t>(inserted);
        }

        int current = inserted;
        while (parent[current] >= 0 && m_directory[parent[current]].colorFlag == 0) {
            const int currentParent = parent[current];
            const int grandParent = parent[currentParent];
            if (currentParent == left(grandParent)) {
                const int uncle = right(grandParent);
                if (uncle >= 0 && m_directory[uncle].colorFlag == 0) {
                    m_directory[currentParent].colorFlag = 1;
                    m_directory[uncle].colorFlag = 1;
                    m_directory[grandParent].colorFlag = 0;
                    current = grandParent;
                } else {
                    if (current == right(currentParent)) {
                        current = currentParent;
                        rotateLeft(current);
                    }
                    m_directory[parent[current]].colorFlag = 1;
                    m_directory[parent[parent[current]]].colorFlag = 0;
                    rotateRight(parent[parent[current]]);
                }
            } else {
                const int uncle = left(grandParent);
                if (uncle >= 0 && m_directory[uncle].colorFlag == 0) {
                    m_directory[currentParent].colorFlag = 1;
                    m_directory[uncle].colorFlag = 1;
                    m_directory[grandParent].colorFlag = 0;
                    current = grandParent;
                } else {
                    if (current == left(currentParent)) {
                        current = currentParent;
                        rotateRight(current);
                    }
                    m_directory[parent[current]].colorFlag = 1;
                    m_directory[parent[parent[current]]].colorFlag = 0;
                    rotateLeft(parent[parent[current]]);
                }
            }
        }
        m_directory[root].colorFlag = 1;
    }

    return static_cast<uint32_t>(root);
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

    m_directory[node.dirIndex].child = buildChildTree(sorted);

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
    const uint32_t totalDifatSectors = static_cast<uint32_t>(m_difatSectors.size());
    uint32_t firstDirSector = totalFatSectors + totalDifatSectors;
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
        firstMiniFatSector = totalFatSectors + totalDifatSectors + dirSectors;
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
    uint32_t firstDifatSector = totalDifatSectors == 0 ? ENDOFCHAIN : totalFatSectors;
    header[offset++] = static_cast<char>(firstDifatSector & 0xFF);
    header[offset++] = static_cast<char>((firstDifatSector >> 8) & 0xFF);
    header[offset++] = static_cast<char>((firstDifatSector >> 16) & 0xFF);
    header[offset++] = static_cast<char>((firstDifatSector >> 24) & 0xFF);

    // Total DIFAT sectors
    header[offset++] = static_cast<char>(totalDifatSectors & 0xFF);
    header[offset++] = static_cast<char>((totalDifatSectors >> 8) & 0xFF);
    header[offset++] = static_cast<char>((totalDifatSectors >> 16) & 0xFF);
    header[offset++] = static_cast<char>((totalDifatSectors >> 24) & 0xFF);

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

        if (dataSize == 0) {
            // CFB 零长度流不占用普通扇区或 mini sector。
            m_directory[dirIndex].startSector = ENDOFCHAIN;
            m_directory[dirIndex].streamSize = 0;
        } else if (dataSize < MINI_STREAM_CUTOFF) {
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
        // miniStreamSectorStart 是大数据扇区数量（数据区内的相对偏移）
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
        // startSector 在 post-processing 中会加 dataOffset 转为绝对扇区号
        m_directory[0].startSector = miniStreamSectorStart;
        m_directory[0].streamSize = static_cast<uint64_t>(m_miniStream.size());

        // 更新 FAT（链式指针使用数据区相对值，post-processing 会加 dataOffset）
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
    // 文件布局：FAT | DIFAT | Directory | MiniFAT | Data。
    // m_fat 目前只有数据扇区的链式条目（每个条目=一个数据扇区的后继指针）
    // 需要补充 FAT/dir/MiniFAT 的条目，并为未使用的数据扇区填 FREESECT
    uint32_t dataSectorCount = static_cast<uint32_t>(m_fat.size());
    uint32_t entriesPerSector = SECTOR_SIZE / 4;

    // 迭代计算 FAT 扇区数（FAT 扇区自身也占条目）
    uint32_t fatSectors = 0;
    uint32_t difatSectors = 0;
    uint32_t totalEntries = 0;
    for (int iter = 0; iter < 32; ++iter) {
        uint32_t newTotal = fatSectors + difatSectors + dirSectors + miniFatSectors + dataSectorCount;
        newTotal = ((newTotal + entriesPerSector - 1) / entriesPerSector) * entriesPerSector;
        uint32_t newFatSectors = newTotal / entriesPerSector;
        uint32_t newDifatSectors =
            newFatSectors <= 109 ? 0 : (newFatSectors - 109 + (entriesPerSector - 2)) / (entriesPerSector - 1);
        if (newFatSectors == fatSectors && newDifatSectors == difatSectors && newTotal == totalEntries) {
            break;
        }
        fatSectors = newFatSectors;
        difatSectors = newDifatSectors;
        totalEntries = newTotal;
    }

    // 构建完整的 FAT 向量
    QVector<uint32_t> fullFat;
    fullFat.reserve(totalEntries);

    // [0..fatSectors-1] FAT sectors → 标记为 FATSECT
    for (uint32_t i = 0; i < fatSectors; ++i) {
        fullFat.append(FATSECT);
    }

    // DIFAT sectors are allocation-table metadata and have their own marker.
    for (uint32_t i = 0; i < difatSectors; ++i) {
        fullFat.append(DIFSECT);
    }

    const uint32_t directoryOffset = fatSectors + difatSectors;
    // Directory sectors → 链式
    for (uint32_t i = 0; i < dirSectors; ++i) {
        if (i < dirSectors - 1) {
            fullFat.append(directoryOffset + i + 1);
        } else {
            fullFat.append(ENDOFCHAIN);
        }
    }

    // MiniFAT sectors 也必须通过 FAT 串成链，不能只依赖物理连续性。
    const uint32_t firstMiniFatOffset = directoryOffset + dirSectors;
    for (uint32_t i = 0; i < miniFatSectors; ++i) {
        fullFat.append(i + 1 < miniFatSectors ? firstMiniFatOffset + i + 1 : ENDOFCHAIN);
    }

    // Data sectors → 来自 m_fat，需要加偏移量
    // m_fat 中的链式指针是从 0 开始的数据扇区索引，需要偏移到文件中的实际位置
    uint32_t dataOffset = fatSectors + difatSectors + dirSectors + miniFatSectors;
    for (uint32_t val : m_fat) {
        if (val == ENDOFCHAIN) {
            fullFat.append(ENDOFCHAIN);
        } else {
            fullFat.append(val + dataOffset);
        }
    }

    // 填充 FREESECT 到刚好 fatSectors 个扇区（不多不少，保证 header 和实际一致）
    uint32_t fatCapacity = fatSectors * entriesPerSector;
    while (static_cast<uint32_t>(fullFat.size()) < fatCapacity) {
        fullFat.append(FREESECT);
    }

    m_fat = fullFat;

    // 头部只能容纳 109 个 FAT sector id，其余 id 以每扇区 127 项串成 DIFAT 链。
    m_difatSectors.clear();
    for (uint32_t difatIndex = 0; difatIndex < difatSectors; ++difatIndex) {
        QByteArray sector(SECTOR_SIZE, static_cast<char>(0xFF));
        const uint32_t firstFatIndex = 109 + difatIndex * (entriesPerSector - 1);
        for (uint32_t item = 0; item < entriesPerSector - 1; ++item) {
            const uint32_t fatIndex = firstFatIndex + item;
            if (fatIndex >= fatSectors) {
                break;
            }
            const int byteOffset = static_cast<int>(item * 4);
            sector[byteOffset] = static_cast<char>(fatIndex & 0xFF);
            sector[byteOffset + 1] = static_cast<char>((fatIndex >> 8) & 0xFF);
            sector[byteOffset + 2] = static_cast<char>((fatIndex >> 16) & 0xFF);
            sector[byteOffset + 3] = static_cast<char>((fatIndex >> 24) & 0xFF);
        }
        const uint32_t next = difatIndex + 1 < difatSectors ? fatSectors + difatIndex + 1 : ENDOFCHAIN;
        const int nextOffset = static_cast<int>((entriesPerSector - 1) * 4);
        sector[nextOffset] = static_cast<char>(next & 0xFF);
        sector[nextOffset + 1] = static_cast<char>((next >> 8) & 0xFF);
        sector[nextOffset + 2] = static_cast<char>((next >> 16) & 0xFF);
        sector[nextOffset + 3] = static_cast<char>((next >> 24) & 0xFF);
        m_difatSectors.append(sector);
    }

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

    if (!m_finalized) {
        finalize();
        m_finalized = true;
    }

    qDebug() << "OLECompoundWriter::saveToFile: Writing to" << filePath << "streams:" << m_streams.size()
             << "directory:" << m_directory.size() << "fat:" << m_fat.size();

    // 确保父目录存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.dir().exists()) {
        qDebug() << "OLECompoundWriter::saveToFile: Creating parent directory:" << fileInfo.path();
        QDir().mkpath(fileInfo.path());
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "OLECompoundWriter::saveToFile: Failed to open file:" << filePath
                   << "error:" << file.errorString() << "exists:" << QFile::exists(filePath)
                   << "isDir:" << QFileInfo(filePath).isDir();
        return false;
    }

    // 写入头部（512 字节）
    QByteArray header;
    serializeFileHeader(header);
    auto writeAll = [&file, &filePath](const QByteArray& bytes) {
        if (file.write(bytes) == bytes.size()) {
            return true;
        }
        qWarning() << "OLECompoundWriter::saveToFile: Short write:" << filePath << file.errorString();
        return false;
    };
    if (!writeAll(header)) {
        file.cancelWriting();
        return false;
    }

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
        if (!writeAll(sectorData)) {
            file.cancelWriting();
            return false;
        }
    }

    for (const QByteArray& sector : m_difatSectors) {
        if (!writeAll(sector)) {
            file.cancelWriting();
            return false;
        }
    }

    // 写入目录扇区
    QByteArray dirData;
    for (const DirectoryEntry& entry : m_directory) {
        QByteArray entryData;
        serializeDirectoryEntry(entry, entryData);
        dirData.append(entryData);
    }
    if (!writeAll(dirData)) {
        file.cancelWriting();
        return false;
    }

    // 写入 Mini-FAT 扇区
    if (!m_miniFat.isEmpty()) {
        QByteArray miniFatData;
        for (uint32_t val : m_miniFat) {
            miniFatData.append(static_cast<char>(val & 0xFF));
            miniFatData.append(static_cast<char>((val >> 8) & 0xFF));
            miniFatData.append(static_cast<char>((val >> 16) & 0xFF));
            miniFatData.append(static_cast<char>((val >> 24) & 0xFF));
        }
        if (!writeAll(miniFatData)) {
            file.cancelWriting();
            return false;
        }
    }

    // 写入数据扇区
    for (const QByteArray& sector : m_dataSectors) {
        if (!writeAll(sector)) {
            file.cancelWriting();
            return false;
        }
    }

    if (!file.commit()) {
        qWarning() << "OLECompoundWriter::saveToFile: Commit failed:" << filePath << file.errorString();
        return false;
    }
    return true;
}

// ---- 序列化辅助方法 ----

}  // namespace EasyKiConverter
