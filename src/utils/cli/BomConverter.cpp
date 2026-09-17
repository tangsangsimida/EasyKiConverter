#include "BomConverter.h"

#include "CliContext.h"
#include "CliExportWaiter.h"
#include "CliPrinter.h"
#include "FileReader.h"
#include "services/ComponentService.h"
#include "services/export/ParallelExportService.h"

#include <QCoreApplication>

namespace EasyKiConverter {

/** @brief 创建 BOM 转换器并绑定 CLI 上下文。 */
BomConverter::BomConverter(CliContext* context, QObject* parent) : BaseConverter(context, parent) {}

/** @brief 读取 BOM、预加载组件并执行导出流程。 */
bool BomConverter::execute() {
    printMessage(QCoreApplication::translate("CliConverter", "开始转换 BOM 表..."));

    // 读取 BOM 表文件
    QString readError;
    QStringList componentIds = FileReader::readBomFile(context()->parser().inputFile(), readError);

    if (!readError.isEmpty()) {
        setError(readError);
        return false;
    }

    if (componentIds.isEmpty()) {
        setError(QCoreApplication::translate("CliConverter", "BOM 表中没有找到有效的元器件编号"));
        return false;
    }

    printMessage(QCoreApplication::translate("CliConverter", "找到 %1 个元器件").arg(componentIds.size()));

    // 创建导出选项
    ExportOptions options = context()->createExportOptions();

    // 设置导出服务
    ParallelExportService* exportService = context()->exportService();
    exportService->setOptions(options);
    exportService->setOutputPath(context()->parser().outputDir());

    // 连接信号
    connect(exportService, &ParallelExportService::preloadCompleted, this, &BomConverter::onPreloadCompleted);
    connect(exportService, &ParallelExportService::progressChanged, this, &BomConverter::onProgressChanged);
    connect(exportService, &ParallelExportService::completed, this, &BomConverter::onExportCompleted);
    connect(exportService, &ParallelExportService::failed, this, &BomConverter::onExportFailed);

    // 统一等待预加载完成。
    CliExportWaiter::waitForPreload(exportService, componentIds);

    // 检查是否预加载成功
    if (errorMessage().isEmpty()) {
        printMessage(QCoreApplication::translate("CliConverter", "预加载完成，开始导出..."));

        // 统一等待导出完成。
        CliExportWaiter::waitForExport(exportService);
    }

    printMessage(QCoreApplication::translate("CliConverter", "\n转换完成: 成功 %1, 失败 %2")
                     .arg(m_successCount)
                     .arg(m_failedCount));

    return m_exportSuccess;
}

/** @brief 输出预加载阶段的成功和失败数量。 */
void BomConverter::onPreloadCompleted(int successCount, int failedCount) {
    printMessage(
        QCoreApplication::translate("CliConverter", "预加载完成: 成功 %1, 失败 %2").arg(successCount).arg(failedCount));
}

/** @brief 根据 CLI 选项输出导出进度。 */
void BomConverter::onProgressChanged(const ExportOverallProgress& progress) {
    if (context()->parser().showProgress()) {
        int percent = progress.overallPercentage();
        printProgressBar(percent);
    }
}

/** @brief 保存导出完成统计并更新最终成功状态。 */
void BomConverter::onExportCompleted(int successCount, int failedCount) {
    m_successCount = successCount;
    m_failedCount = failedCount;
    m_exportSuccess = (failedCount == 0);
    m_exportFinished = true;
}

/** @brief 保存导出失败信息并终止成功状态。 */
void BomConverter::onExportFailed(const QString& error) {
    setError(error);
    m_exportSuccess = false;
    m_exportFinished = true;
}

}  // namespace EasyKiConverter
