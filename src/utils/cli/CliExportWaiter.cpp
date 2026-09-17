#include "CliExportWaiter.h"

#include "services/export/ParallelExportService.h"

#include <QEventLoop>

namespace EasyKiConverter {

/** @brief 等待预加载完成，统一 CLI 转换器的事件循环边界。 */
void CliExportWaiter::waitForPreload(ParallelExportService* service, const QStringList& componentIds) {
    if (service == nullptr)
        return;

    QEventLoop preloadLoop;
    QObject::connect(service, &ParallelExportService::preloadCompleted, &preloadLoop, &QEventLoop::quit);
    QObject::connect(service, &ParallelExportService::failed, &preloadLoop, &QEventLoop::quit);
    service->startPreload(componentIds);
    preloadLoop.exec();
}

/** @brief 等待导出完成，统一 CLI 转换器的事件循环边界。 */
void CliExportWaiter::waitForExport(ParallelExportService* service) {
    if (service == nullptr)
        return;

    QEventLoop exportLoop;
    QObject::connect(service, &ParallelExportService::completed, &exportLoop, &QEventLoop::quit);
    QObject::connect(service, &ParallelExportService::failed, &exportLoop, &QEventLoop::quit);
    service->startExport();
    exportLoop.exec();
}

}  // namespace EasyKiConverter
