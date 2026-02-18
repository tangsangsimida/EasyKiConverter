#ifndef EASYKICONVERTER_PIPELINECLEANUP_H
#define EASYKICONVERTER_PIPELINECLEANUP_H

#include "../../../models/ComponentData.h"
#include "../../../models/ComponentExportStatus.h"

#include <QList>
#include <QMap>
#include <QSharedPointer>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 流水线资源清理器
 */
class PipelineCleanup {
public:
    static void emergencyCleanup(QList<QSharedPointer<ComponentExportStatus>>& completedStatuses,
                                 QMap<QString, QSharedPointer<ComponentData>>& preloadedData,
                                 QList<SymbolData>& symbols,  // 改为 SymbolData
                                 QString& tempDir);

    static bool removeTempDir(const QString& tempDir);
};

}  // namespace EasyKiConverter

#endif  // EASYKICONVERTER_PIPELINECLEANUP_H
