#include "Model3DExportStage.h"

#include "Model3DExportWorker.h"
#include "core/kicad/Exporter3DModel.h"

#include <QDebug>
#include <QDir>
#include <QPointer>

namespace EasyKiConverter {

Model3DExportStage::Model3DExportStage(QObject* parent)
    : ExportTypeStage("Model3D", 2, parent) {
}

Model3DExportStage::~Model3DExportStage() {
    cancel();
}

void Model3DExportStage::start(const QStringList& componentIds,
                              const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isExporting.load()) {
        qWarning() << "Model3DExportStage: Export already in progress";
        return;
    }

    if (componentIds.isEmpty()) {
        qWarning() << "Model3DExportStage: No components to export";
        emit completed(0, 0, 0);
        return;
    }

    // 调用基类的初始化方法（设置状态）
    ExportTypeStage::start(componentIds, cachedData);

    // 构建输出目录：outputPath/libName.3dmodels/
    QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
    QString baseOutputDir = m_options.outputPath;
    if (baseOutputDir.isEmpty()) {
        baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString outputDir = baseOutputDir + QDir::separator() + libName + QStringLiteral(".3dmodels");

    // 确保输出目录存在
    QDir dir;
    if (!dir.mkpath(outputDir)) {
        qCritical() << "Model3DExportStage: Failed to create output directory:" << outputDir;
        m_isExporting.store(false);
        emit completed(0, 0, 0);
        return;
    }

    // 设置临时文件管理器
    m_tempManager.setOutputPath(outputDir);

    // 预创建所有组件的临时文件路径
    m_tempPaths.clear();
    m_finalPaths.clear();
    for (const QString& componentId : componentIds) {
        QString finalPath = outputDir + QDir::separator() + componentId + QStringLiteral(".wrl");
        QString tempPath = m_tempManager.createTempFilePath(componentId, QStringLiteral(".wrl"));
        if (!tempPath.isEmpty()) {
            m_tempPaths[componentId] = tempPath;
            m_finalPaths[componentId] = finalPath;
        }
    }

    m_isExporting.store(true);
}

void Model3DExportStage::cancel() {
    if (!m_isExporting.load()) {
        return;
    }

    qDebug() << "Model3DExportStage: Cancelling...";

    // 设置取消标志
    m_cancelled.store(true);

    // 回滚所有临时文件
    m_tempManager.rollbackAll();

    m_isExporting.store(false);
    m_isRunning.store(false);

    qDebug() << "Model3DExportStage: Cancelled";
}

void Model3DExportStage::commitTempFile(const QString& tempPath, const QString& finalPath) {
    if (tempPath.isEmpty() || finalPath.isEmpty()) {
        return;
    }

    if (m_tempManager.commit(finalPath)) {
        qDebug() << "Model3DExportStage: Committed temp file:" << finalPath;
    } else {
        qWarning() << "Model3DExportStage: Failed to commit temp file:" << tempPath;
    }
}

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

    // 设置临时文件路径
    if (m_tempPaths.contains(componentId)) {
        exportWorker->setTempPath(m_tempPaths[componentId]);
    }

    // 使用QPointer防止stage已销毁时lambda访问悬空指针
    QPointer<Model3DExportStage> stagePtr(this);
    connect(exportWorker,
            &Model3DExportWorker::completed,
            this,
            [stagePtr, exportWorker, componentId](const QString&, bool success, const QString& error) {
                if (!stagePtr) {
                    return;
                }

                // 如果成功，提交临时文件
                if (success && stagePtr->m_finalPaths.contains(componentId)) {
                    QString tempPath = stagePtr->m_tempPaths.value(componentId);
                    QString finalPath = stagePtr->m_finalPaths.value(componentId);
                    stagePtr->commitTempFile(tempPath, finalPath);
                }

                stagePtr->completeItemProgress(exportWorker, componentId, success, error);
            });

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
