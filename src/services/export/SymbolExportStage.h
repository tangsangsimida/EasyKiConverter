#pragma once

#include "ExportProgress.h"
#include "ExportTypeStage.h"
#include "TempFileManager.h"

#include <QThread>

namespace EasyKiConverter {

/**
 * @brief 符号库导出阶段
 *
 * 继承自 ExportTypeStage，但覆盖 start() 方法以实现库级别的合并导出。
 *
 * 目录结构:
 *   <outputPath>/
 *   ├── <libName>.kicad_sym  ← 符号库文件（合并所有符号）
 *   └── .tmp/                ← 临时文件（导出完成后自动清理）
 *
 * 工作流程（库级别）:
 * 1. start() 被调用时，聚合所有元器件的符号数据
 * 2. 在临时目录创建符号库文件
 * 3. 调用 ExporterSymbol::exportSymbolLibrary() 一次性导出所有符号
 * 4. 导出成功: 将临时文件重命名为最终路径
 * 5. 导出失败/取消: 删除所有临时文件
 *
 * 注意：此阶段不使用 worker 池模式，而是在工作线程中同步完成所有导出。
 *      为保持类可实例化，提供了 createWorker() 和 startWorker() 的空实现。
 */
class SymbolExportStage : public ExportTypeStage {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit SymbolExportStage(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~SymbolExportStage() override;

    /**
     * @brief 设置导出选项
     * @param options 导出选项配置
     */
    void setOptions(const ExportOptions& options) {
        m_options = options;
    }

    /**
     * @brief 开始符号库导出
     * @param componentIds 需要导出的元器件ID列表
     * @param cachedData 预加载的元器件数据（key: componentId, value: ComponentData）
     *
     * 覆盖基类方法，实现库级别的合并导出。
     * 不使用 worker 池，而是在工作线程中同步完成。
     */
    void start(const QStringList& componentIds,
               const QMap<QString, QSharedPointer<ComponentData>>& cachedData) override;

    /**
     * @brief 取消导出
     */
    void cancel() override;

protected:
    // 以下方法覆盖基类纯虚函数，但在此实现中不使用
    QObject* createWorker() override {
        return nullptr;
    }

    void startWorker(QObject*, const QString&, const QSharedPointer<ComponentData>&) override {}

private:
    /**
     * @brief 在工作线程中执行库级别的导出
     */
    void doLibraryExport(const QStringList& componentIds,
                         const QMap<QString, QSharedPointer<ComponentData>>& cachedData);

    struct ExportOptions m_options;  ///< 导出选项（库级别）
    TempFileManager m_tempManager;  ///< 临时文件管理器
    std::atomic<bool> m_isExporting{false};  ///< 是否正在导出
    QThread* m_workerThread = nullptr;  ///< 工作线程指针
};

}  // namespace EasyKiConverter

// 我草了老铁，我感觉我的傻福朋友最近有点开智了，
// 每当深夜就开始跟我讨论人生的意义到底是什么，
// 我们每天日复一日的过着单调的生活，
// 一年到底是过了365天还是把一天过了365遍。
// 我说朋友，重复一天的生活有什么不好的，
// 我怎么感觉每天都有不同的报应在等着我。
// 之前有人跟我说我这辈子也就这样了，
// 我说谢谢你呀，我的生活真的不会变得更糟糕了吗？
// 但是我仔细回味回味感觉有些不对。
// 为什么我平时傻福一样的朋友只会在晚上刷新出这些有哲理的问题，
// 朋友你要是再在你看完新玩家招募动画以后，
// 在贤者时间跟我聊这些问题！
// 我就会在地球online把你下载到本地，
// 狠狠的击破你的弱点！
// 咕咕嘎嘎！！！
