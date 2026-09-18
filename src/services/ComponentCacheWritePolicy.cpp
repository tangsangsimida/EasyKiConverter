#include "ComponentCacheWritePolicy.h"

namespace EasyKiConverter {

/**
 * @brief 判断异步缓存写入是否仍属于有效缓存上下文。
 * @details 同步写入请求的 expectedGeneration 为零时保持原有的无条件放行行为。
 */
bool ComponentCacheWritePolicy::isAllowed(uint64_t currentGeneration,
                                          uint64_t expectedGeneration,
                                          const CacheTombstoneRegistry& tombstones,
                                          const QString& componentId) {
    if (expectedGeneration == 0) {
        return true;
    }
    return currentGeneration == expectedGeneration && !tombstones.isBlocked(componentId);
}

}  // namespace EasyKiConverter
