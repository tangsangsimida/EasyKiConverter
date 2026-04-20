#include "ComponentConverter.h"

#include "CliContext.h"
#include "services/ComponentService.h"
#include "services/export/ParallelExportService.h"

#include <QEventLoop>

namespace EasyKiConverter {

ComponentConverter::ComponentConverter(CliContext* context, QObject* parent) : BaseConverter(context, parent) {}

bool ComponentConverter::execute() {
    printMessage("开始转换单个元器件...");

    QString componentId = context()->parser().componentId();
    if (componentId.isEmpty()) {
        setError("未指定元器件编号");
        return false;
    }

    printMessage(QString("元器件编号: %1").arg(componentId));

    // 创建导出选项
    ExportOptions options = context()->createExportOptions();

    // 设置导出服务
    ParallelExportService* exportService = context()->exportService();
    exportService->setOptions(options);
    exportService->setOutputPath(context()->parser().outputDir());

    // 连接信号
    connect(exportService, &ParallelExportService::preloadCompleted,
            this, &ComponentConverter::onExportCompleted);
    connect(exportService, &ParallelExportService::completed,
            this, &ComponentConverter::onExportCompleted);
    connect(exportService, &ParallelExportService::failed,
            this, &ComponentConverter::onExportFailed);

    // 创建事件循环等待预加载完成
    QEventLoop preloadLoop;
    connect(exportService, &ParallelExportService::preloadCompleted, &preloadLoop, &QEventLoop::quit);
    connect(exportService, &ParallelExportService::failed, &preloadLoop, &QEventLoop::quit);

    // 开始预加载
    QStringList componentIds;
    componentIds.append(componentId);
    exportService->startPreload(componentIds);

    // 等待预加载完成
    preloadLoop.exec();

    // 检查是否预加载成功
    if (errorMessage().isEmpty()) {
        printMessage("预加载完成，开始导出...");

        // 创建事件循环等待导出完成
        QEventLoop exportLoop;
        connect(exportService, &ParallelExportService::completed, &exportLoop, &QEventLoop::quit);
        connect(exportService, &ParallelExportService::failed, &exportLoop, &QEventLoop::quit);

        // 开始导出
        exportService->startExport();

        // 等待导出完成
        exportLoop.exec();
    }

    printMessage(QString("转换完成: 成功 %1, 失败 %2").arg(m_successCount).arg(m_failedCount));

    return m_exportSuccess;
}

void ComponentConverter::onExportCompleted(int successCount, int failedCount) {
    m_successCount = successCount;
    m_failedCount = failedCount;
    m_exportSuccess = (failedCount == 0);
    m_exportFinished = true;
}

void ComponentConverter::onExportFailed(const QString& error) {
    setError(error);
    m_exportSuccess = false;
    m_exportFinished = true;
}

}  // namespace EasyKiConverter
