#include "Model3DExportStage.h"

#include "Model3DExportWorker.h"

namespace EasyKiConverter {

Model3DExportStage::Model3DExportStage(QObject* parent) : ExportTypeStage("Model3D", 2, parent) {}

QObject* Model3DExportStage::createWorker() {
    return new Model3DExportWorker();
}

void Model3DExportStage::startWorker(QObject* worker,
                                     const QString& componentId,
                                     const QSharedPointer<ComponentData>& data) {
    auto* exportWorker = qobject_cast<Model3DExportWorker*>(worker);
    if (!exportWorker) {
        qWarning() << "Model3DExportStage: Failed to cast worker to Model3DExportWorker";
        return;
    }

    exportWorker->setOptions(m_options);
    exportWorker->setData(componentId, data, m_options);

    connect(exportWorker,
            &Model3DExportWorker::completed,
            this,
            [this, componentId](const QString&, bool success, const QString& error) {
                completeItemProgress(componentId, success, error);
            });

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
