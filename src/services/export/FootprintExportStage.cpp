#include "FootprintExportStage.h"

#include "FootprintExportWorker.h"

namespace EasyKiConverter {

FootprintExportStage::FootprintExportStage(QObject* parent) : ExportTypeStage("Footprint", 3, parent) {}

QObject* FootprintExportStage::createWorker() {
    return new FootprintExportWorker();
}

void FootprintExportStage::startWorker(QObject* worker,
                                       const QString& componentId,
                                       const QSharedPointer<ComponentData>& data) {
    auto* exportWorker = qobject_cast<FootprintExportWorker*>(worker);
    if (!exportWorker) {
        qWarning() << "FootprintExportStage: Failed to cast worker to FootprintExportWorker";
        return;
    }

    exportWorker->setOptions(m_options);
    exportWorker->setData(componentId, data, m_options);

    connect(exportWorker,
            &FootprintExportWorker::completed,
            this,
            [this, componentId](const QString&, bool success, const QString& error) {
                completeItemProgress(componentId, success, error);
            });

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
