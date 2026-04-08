#include "PreviewImagesExportStage.h"

#include "PreviewImagesExportWorker.h"

namespace EasyKiConverter {

PreviewImagesExportStage::PreviewImagesExportStage(QObject* parent) : ExportTypeStage("PreviewImages", 4, parent) {}

QObject* PreviewImagesExportStage::createWorker() {
    return new PreviewImagesExportWorker();
}

void PreviewImagesExportStage::startWorker(QObject* worker,
                                           const QString& componentId,
                                           const QSharedPointer<ComponentData>& data) {
    auto* exportWorker = qobject_cast<PreviewImagesExportWorker*>(worker);
    if (!exportWorker) {
        qWarning() << "PreviewImagesExportStage: Failed to cast worker to PreviewImagesExportWorker";
        return;
    }

    exportWorker->setOptions(m_options);
    exportWorker->setData(componentId, data, m_options);

    connect(exportWorker,
            &PreviewImagesExportWorker::completed,
            this,
            [this, componentId](const QString&, bool success, const QString& error) {
                completeItemProgress(componentId, success, error);
            });

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
