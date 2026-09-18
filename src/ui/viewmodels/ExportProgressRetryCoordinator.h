#pragma once

class QString;

namespace EasyKiConverter {

class ExportProgressViewModel;

/**
 * @brief 协调导出结果中的单项和批量失败重试。
 *
 * 该类负责筛选失败元件、重置结果项并重新启动预加载，不改变视图模型对外接口。
 */
class ExportProgressRetryCoordinator final {
public:
    /** @brief 重试指定元件的失败导出。 */
    static void retryComponent(ExportProgressViewModel& owner, const QString& componentId);

    /** @brief 重试当前结果列表中的全部失败元件。 */
    static void retryFailedComponents(ExportProgressViewModel& owner);
};

}  // namespace EasyKiConverter
