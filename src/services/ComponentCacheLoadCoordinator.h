#pragma once

#include <QString>

namespace EasyKiConverter {

class ComponentCacheService;
class ComponentService;

/**
 * @brief 协调元器件缓存的异步加载流程。
 * @details 负责后台读取、代次校验、网络回退、状态合并以及缓存结果信号转发。
 */
class ComponentCacheLoadCoordinator final {
public:
    /** @brief 启动指定元器件的异步缓存加载。 */
    static void load(ComponentService& owner,
                     const QString& normalizedId,
                     bool fetch3DModel,
                     ComponentCacheService* cache);
};

}  // namespace EasyKiConverter
