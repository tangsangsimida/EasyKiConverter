#include "PipelineCleanup.h"

#include "utils/PathSecurity.h"

#include <QDebug>

namespace EasyKiConverter {

void PipelineCleanup::emergencyCleanup(

    QList<QSharedPointer<ComponentExportStatus>>& completedStatuses,

    QMap<QString, QSharedPointer<ComponentData>>& preloadedData,

    QList<SymbolData>& symbols,  // 改为 SymbolData

    QString& tempDir) {
    qDebug() << "PipelineCleanup: Performing emergency cleanup...";

    for (const auto& status : completedStatuses) {
        if (status) {
            status->clearIntermediateData(false);
            status->clearStepData();
        }
    }
    completedStatuses.clear();
    preloadedData.clear();
    // 不清空 symbols，因为它们已被写入临时文件，需要在合并后再清理
    // 不清理 tempDir，因为需要用它来合并符号库
}

bool PipelineCleanup::removeTempDir(const QString& tempDir) {
    if (tempDir.isEmpty())
        return true;

    if (PathSecurity::safeRemoveRecursively(tempDir)) {
        qDebug() << "PipelineCleanup: Temp folder removed safely:" << tempDir;
        return true;
    } else {
        qWarning() << "PipelineCleanup: Failed to remove temp folder:" << tempDir;
        return false;
    }
}

}  // namespace EasyKiConverter
