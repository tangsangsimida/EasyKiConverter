#include "BomConverter.h"

#include "CliContext.h"
#include "CliPrinter.h"
#include "FileReader.h"
#include "services/ComponentService.h"
#include "services/export/ParallelExportService.h"

#include <QEventLoop>

namespace EasyKiConverter {

BomConverter::BomConverter(CliContext* context, QObject* parent) : BaseConverter(context, parent) {}

bool BomConverter::execute() {
    printMessage("开始转换 BOM 表...");

    // 读取 BOM 表文件
    QString readError;
    QStringList componentIds = FileReader::readBomFile(context()->parser().inputFile(), readError);

    if (!readError.isEmpty()) {
        setError(readError);
        return false;
    }

    if (componentIds.isEmpty()) {
        setError("BOM 表中没有找到有效的元器件编号");
        return false;
    }

    printMessage(QString("找到 %1 个元器件").arg(componentIds.size()));

    // 创建导出选项
    ExportOptions options = context()->createExportOptions();

    // 设置导出服务
    ParallelExportService* exportService = context()->exportService();
    exportService->setOptions(options);
    exportService->setOutputPath(context()->parser().outputDir());

    // 连接信号
    connect(exportService, &ParallelExportService::preloadCompleted,
            this, &BomConverter::onPreloadCompleted);
    connect(exportService, &ParallelExportService::progressChanged,
            this, &BomConverter::onProgressChanged);
    connect(exportService, &ParallelExportService::completed,
            this, &BomConverter::onExportCompleted);
    connect(exportService, &ParallelExportService::failed,
            this, &BomConverter::onExportFailed);

    // 创建事件循环等待预加载完成
    QEventLoop preloadLoop;
    connect(exportService, &ParallelExportService::preloadCompleted, &preloadLoop, &QEventLoop::quit);
    connect(exportService, &ParallelExportService::failed, &preloadLoop, &QEventLoop::quit);

    // 开始预加载
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

    printMessage(QString("\n转换完成: 成功 %1, 失败 %2").arg(m_successCount).arg(m_failedCount));

    return m_exportSuccess;
}

void BomConverter::onPreloadCompleted(int successCount, int failedCount) {
    printMessage(QString("预加载完成: 成功 %1, 失败 %2").arg(successCount).arg(failedCount));
}

void BomConverter::onProgressChanged(const ExportOverallProgress& progress) {
    if (context()->parser().showProgress()) {
        int total = progress.totalComponents;
        int completed = progress.totalSuccessCount() + progress.totalFailedCount();
        int percent = total > 0 ? (completed * 100 / total) : 0;
        printProgressBar(percent);
    }
}

void BomConverter::onExportCompleted(int successCount, int failedCount) {
    m_successCount = successCount;
    m_failedCount = failedCount;
    m_exportSuccess = (failedCount == 0);
    m_exportFinished = true;
}

void BomConverter::onExportFailed(const QString& error) {
    setError(error);
    m_exportSuccess = false;
    m_exportFinished = true;
}

}  // namespace EasyKiConverter
