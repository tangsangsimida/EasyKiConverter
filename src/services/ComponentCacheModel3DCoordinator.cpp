#include "ComponentCacheModel3DCoordinator.h"

#include "CacheDataValidator.h"
#include "ComponentCacheService.h"
#include "Model3DCacheFileStore.h"
#include "utils/logging/LogMacros.h"

#include <QFileInfo>
#include <QMutexLocker>

namespace EasyKiConverter {

/** @brief 保存协作者所属的元器件缓存服务。 */
ComponentCacheModel3DCoordinator::ComponentCacheModel3DCoordinator(ComponentCacheService& owner) : m_owner(owner) {}

/** @brief 在目录迁移锁和服务锁保护下检查三维模型缓存。 */
bool ComponentCacheModel3DCoordinator::hasCached(const QString& uuid, const QString& extension) const {
    QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
    QMutexLocker locker(&m_owner.m_mutex);
    const QString path = m_owner.model3DPath(uuid, extension);
    return !Model3DCacheFileStore::read(path, extension).isEmpty();
}

/** @brief 在目录迁移锁和服务锁保护下读取三维模型缓存。 */
QByteArray ComponentCacheModel3DCoordinator::load(const QString& uuid, const QString& extension) const {
    QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
    QMutexLocker locker(&m_owner.m_mutex);

    const QString path = m_owner.model3DPath(uuid, extension);
    if (!QFileInfo::exists(path)) {
        return QByteArray();
    }
    return Model3DCacheFileStore::read(path, extension);
}

/** @brief 校验代次后写入三维模型，并触发统一磁盘配额策略。 */
void ComponentCacheModel3DCoordinator::save(const QString& uuid,
                                            const QByteArray& data,
                                            const QString& extension,
                                            uint64_t expectedGeneration) {
    if (!CacheDataValidator::isUsableModel3D(data, extension)) {
        return;
    }

    QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
    if (expectedGeneration != 0 && m_owner.m_cacheGeneration.load() != expectedGeneration) {
        return;
    }

    QString path;
    {
        QMutexLocker locker(&m_owner.m_mutex);
        m_owner.ensureModel3DCacheDir();
        path = m_owner.model3DPath(uuid, extension);
        if (path.isEmpty()) {
            return;
        }
    }

    if (Model3DCacheFileStore::write(path, data, extension)) {
        LOG_DEBUG(LogModule::Core, "Saved 3D model to disk: {}", path);
        m_owner.enforceDiskCacheLimit();
    } else {
        LOG_WARN(LogModule::Core, "Failed to write 3D model: {}", path);
    }
    // 三维模型数据量大，不写入一级内存缓存。
}

/** @brief 在目录迁移锁保护下复制三维模型缓存文件。 */
bool ComponentCacheModel3DCoordinator::copyToFile(const QString& uuid,
                                                  const QString& extension,
                                                  const QString& destinationPath) const {
    if (uuid.isEmpty() || extension.isEmpty() || destinationPath.isEmpty()) {
        return false;
    }

    QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
    const QString sourcePath = m_owner.model3DPath(uuid, extension);
    if (sourcePath.isEmpty()) {
        LOG_WARN(LogModule::Core, "copyModel3DToFile: Source file does not exist: {}", sourcePath);
        return false;
    }
    if (Model3DCacheFileStore::copy(sourcePath, destinationPath, extension)) {
        LOG_DEBUG(LogModule::Core, "Copied 3D model from cache to: {}", destinationPath);
        return true;
    }
    LOG_WARN(LogModule::Core, "copyModel3DToFile: Failed to copy {} -> {}", sourcePath, destinationPath);
    return false;
}

}  // namespace EasyKiConverter
