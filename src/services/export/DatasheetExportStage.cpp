#include "DatasheetExportStage.h"

#include "DatasheetExportWorker.h"

namespace EasyKiConverter {

DatasheetExportStage::DatasheetExportStage(QObject* parent) : ExportTypeStage("Datasheet", 2, parent) {}

QObject* DatasheetExportStage::createWorker() {
    return new DatasheetExportWorker();
}

void DatasheetExportStage::startWorker(QObject* worker,
                                       const QString& componentId,
                                       const QSharedPointer<ComponentData>& data) {
    auto* exportWorker = qobject_cast<DatasheetExportWorker*>(worker);
    if (!exportWorker) {
        qWarning() << "DatasheetExportStage: Failed to cast worker to DatasheetExportWorker";
        return;
    }

    exportWorker->setOptions(m_options);
    exportWorker->setData(componentId, data, m_options);

    connect(exportWorker,
            &DatasheetExportWorker::completed,
            this,
            [this, componentId](const QString&, bool success, const QString& error) {
                completeItemProgress(componentId, success, error);
            });

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
