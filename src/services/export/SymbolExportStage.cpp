#include "SymbolExportStage.h"

#include "SymbolExportWorker.h"

namespace EasyKiConverter {

SymbolExportStage::SymbolExportStage(QObject* parent) : ExportTypeStage("Symbol", 3, parent) {}

QObject* SymbolExportStage::createWorker() {
    return new SymbolExportWorker();
}

void SymbolExportStage::startWorker(QObject* worker,
                                    const QString& componentId,
                                    const QSharedPointer<ComponentData>& data) {
    auto* exportWorker = qobject_cast<SymbolExportWorker*>(worker);
    if (!exportWorker) {
        qWarning() << "SymbolExportStage: Failed to cast worker to SymbolExportWorker";
        return;
    }

    exportWorker->setOptions(m_options);
    exportWorker->setData(componentId, data, m_options);

    // 连接完成信号到阶段完成处理
    connect(exportWorker,
            &SymbolExportWorker::completed,
            this,
            [this, componentId](const QString&, bool success, const QString& error) {
                completeItemProgress(componentId, success, error);
            });

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
