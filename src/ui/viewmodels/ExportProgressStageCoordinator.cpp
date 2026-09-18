#include "ExportProgressStageCoordinator.h"

#include "ExportProgressViewModel.h"

#include <QtGlobal>

namespace EasyKiConverter {

namespace {

constexpr int kFetchWeight = 30;
constexpr int kProcessWeight = 50;
constexpr int kWriteWeight = 20;

const QString kSymbolType = QStringLiteral("Symbol");
const QString kFootprintType = QStringLiteral("Footprint");
const QString kModel3DType = QStringLiteral("Model3D");
const QString kPreviewType = QStringLiteral("PreviewImages");
const QString kDatasheetType = QStringLiteral("Datasheet");

}  // namespace

// 判断指定导出类型是否已在当前选项中启用。
bool ExportProgressStageCoordinator::isTypeEnabled(const ExportProgressViewModel& viewModel, const QString& typeName) {
    if (typeName == kSymbolType)
        return viewModel.m_exportSymbolEnabled;
    if (typeName == kFootprintType)
        return viewModel.m_exportFootprintEnabled;
    if (typeName == kModel3DType)
        return viewModel.m_exportModel3DEnabled;
    if (typeName == kPreviewType)
        return viewModel.m_exportPreviewEnabled;
    if (typeName == kDatasheetType)
        return viewModel.m_exportDatasheetEnabled;
    return false;
}

// 计算一组导出类型的平均进度。
int ExportProgressStageCoordinator::averageTypeProgress(const ExportOverallProgress& progress,
                                                        const QStringList& typeNames) {
    int sum = 0;
    int count = 0;
    for (const QString& typeName : typeNames) {
        const auto it = progress.exportTypeProgress.constFind(typeName);
        if (it == progress.exportTypeProgress.constEnd())
            continue;
        sum += it.value().percentage();
        ++count;
    }
    return count == 0 ? 0 : qBound(0, sum / count, 100);
}

// 计算启用导出类型的阶段进度，未启用时视为已完成。
int ExportProgressStageCoordinator::stageTypeProgress(const ExportProgressViewModel& viewModel,
                                                      const ExportOverallProgress& progress,
                                                      const QStringList& typeNames) {
    for (const QString& typeName : typeNames) {
        if (isTypeEnabled(viewModel, typeName))
            return averageTypeProgress(progress, typeNames);
    }
    return 100;
}

// 按抓取、处理和写入三个阶段的权重合并总体进度。
int ExportProgressStageCoordinator::weightedOverallProgress(const ExportProgressViewModel& viewModel) {
    const int weighted = (viewModel.m_fetchProgress * kFetchWeight) + (viewModel.m_processProgress * kProcessWeight) +
                         (viewModel.m_writeProgress * kWriteWeight);
    return qBound(0, weighted / 100, 100);
}

void ExportProgressStageCoordinator::handlePreloadProgress(ExportProgressViewModel& viewModel,
                                                           const PreloadProgress& progress) {
    QString statusText = QString("Preloading... %1/%2").arg(progress.completedCount).arg(progress.totalCount);
    if (!progress.currentComponentId.isEmpty())
        statusText += QString(" (%1)").arg(progress.currentComponentId);
    viewModel.setStatus(statusText);
    viewModel.m_fetchProgress = qBound(0, progress.percentage(), 100);
    viewModel.m_processProgress = 0;
    viewModel.m_writeProgress = 0;
    viewModel.setProgress(weightedOverallProgress(viewModel));
    emit viewModel.stageProgressChanged();
}

/** @brief 根据当前导出阶段更新抓取、处理、写入进度和状态文本。 */
void ExportProgressStageCoordinator::handleProgress(ExportProgressViewModel& viewModel,
                                                    const ExportOverallProgress& progress) {
    // 按当前阶段更新三段式进度，并确保后续阶段不超过前置阶段。
    switch (progress.currentStage) {
        case ExportOverallProgress::Stage::Preloading:
            viewModel.setStatus("Preloading components...");
            viewModel.m_fetchProgress = progress.preloadProgress.percentage();
            viewModel.m_processProgress = 0;
            viewModel.m_writeProgress = 0;
            viewModel.setProgress(weightedOverallProgress(viewModel));
            break;
        case ExportOverallProgress::Stage::Exporting: {
            viewModel.setStatus("Exporting components...");
            viewModel.m_fetchProgress =
                stageTypeProgress(viewModel, progress, {kPreviewType, kDatasheetType, kModel3DType});
            const int rawProcessProgress =
                stageTypeProgress(viewModel, progress, {kSymbolType, kFootprintType, kModel3DType});
            const int rawWriteProgress = stageTypeProgress(
                viewModel, progress, {kSymbolType, kFootprintType, kModel3DType, kPreviewType, kDatasheetType});
            viewModel.m_processProgress = qMin(rawProcessProgress, viewModel.m_fetchProgress);
            viewModel.m_writeProgress = qMin(rawWriteProgress, viewModel.m_processProgress);
            viewModel.setProgress(weightedOverallProgress(viewModel));
            break;
        }
        case ExportOverallProgress::Stage::Completed:
            viewModel.m_fetchProgress = 100;
            viewModel.m_processProgress = 100;
            viewModel.m_writeProgress = 100;
            viewModel.setProgress(100);
            viewModel.setStatus("Export completed");
            break;
        case ExportOverallProgress::Stage::Cancelled:
            viewModel.m_fetchProgress = 0;
            viewModel.m_processProgress = 0;
            viewModel.m_writeProgress = 0;
            viewModel.setProgress(0);
            viewModel.setStatus("Export cancelled");
            break;
        case ExportOverallProgress::Stage::Failed:
            viewModel.m_fetchProgress = 0;
            viewModel.m_processProgress = 0;
            viewModel.m_writeProgress = 0;
            viewModel.setStatus("Export failed");
            break;
        default:
            break;
    }
    emit viewModel.stageProgressChanged();
}

}  // namespace EasyKiConverter
