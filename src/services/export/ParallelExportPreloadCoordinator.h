#ifndef PARALLELEXPORTPRELOADCOORDINATOR_H
#define PARALLELEXPORTPRELOADCOORDINATOR_H

#include <QList>
#include <QMap>

namespace EasyKiConverter {

class ComponentData;
class ParallelExportService;

/**
 * @brief 协调并行导出前的组件数据预加载。
 * @details 统一处理网络批量结果、磁盘缓存回退、严格数据校验和预加载进度收敛。
 */
class ParallelExportPreloadCoordinator final {
public:
    /** @brief 处理没有网络服务时的磁盘缓存分批预加载。 */
    static void processNextBatch(ParallelExportService& service);

    /** @brief 合并网络批量获取结果并完成预加载统计。 */
    static void handleCollectedData(ParallelExportService& service,
                                    const QList<ComponentData>& componentDataList,
                                    const QMap<QString, QString>& failedComponents);
};

}  // namespace EasyKiConverter

#endif  // PARALLELEXPORTPRELOADCOORDINATOR_H
