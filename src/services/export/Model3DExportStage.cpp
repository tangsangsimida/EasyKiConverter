#include "Model3DExportStage.h"

#include "Model3DExportWorker.h"
#include "core/kicad/Exporter3DModel.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    m_componentPaths.clear();
    for (const QString& componentId : componentIds) {
        TempFilePaths paths;
        paths.wrlFinalPath = outputDir + QDir::separator() + componentId + QStringLiteral(".wrl");
        paths.stepFinalPath = outputDir + QDir::separator() + componentId + QStringLiteral(".step");
        paths.wrlTempPath = m_tempManager.createTempFilePath(componentId + QStringLiteral("_wrl"), QStringLiteral(".wrl"));
        paths.stepTempPath =
            m_tempManager.createTempFilePath(componentId + QStringLiteral("_step"), QStringLiteral(".step"));

        if (!paths.wrlTempPath.isEmpty() && !paths.stepTempPath.isEmpty()) {
            m_componentPaths[componentId] = paths;
        }
    }

    // 调用基类的初始化方法（在路径准备完毕后再启动 worker，避免竞态）
    ExportTypeStage::start(componentIds, cachedData);

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

bool Model3DExportStage::commitTempFile(const QString& tempPath, const QString& finalPath) {
    if (tempPath.isEmpty() || finalPath.isEmpty()) {
        return false;
    }

    QFileInfo finalInfo(finalPath);
    if (!finalInfo.absoluteDir().exists() && !QDir().mkpath(finalInfo.absolutePath())) {
        qWarning() << "Model3DExportStage: Failed to create output dir for:" << finalPath;
        return false;
    }

    if (QFile::exists(finalPath) && !QFile::remove(finalPath)) {
        qWarning() << "Model3DExportStage: Failed to remove existing file:" << finalPath;
        return false;
    }

    if (QFile::rename(tempPath, finalPath)) {
        qDebug() << "Model3DExportStage: Committed temp file:" << finalPath;
        return true;
    }

    qWarning() << "Model3DExportStage: Failed to commit temp file:" << tempPath;
    return false;
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
    if (m_componentPaths.contains(componentId)) {
        const TempFilePaths& paths = m_componentPaths[componentId];
        exportWorker->setOutputPaths({paths.wrlTempPath, paths.stepTempPath});
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
                if (success && stagePtr->m_componentPaths.contains(componentId)) {
                    const TempFilePaths paths = stagePtr->m_componentPaths.value(componentId);
                    const bool wrlCommitted = stagePtr->commitTempFile(paths.wrlTempPath, paths.wrlFinalPath);
                    const bool stepCommitted = stagePtr->commitTempFile(paths.stepTempPath, paths.stepFinalPath);
                    if (!wrlCommitted || !stepCommitted) {
                        success = false;
                    }
                }

                stagePtr->completeItemProgress(exportWorker, componentId, success, error);
            });

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
