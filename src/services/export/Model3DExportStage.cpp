#include "Model3DExportStage.h"

#include "Model3DExportWorker.h"
#include "core/kicad/Exporter3DModel.h"
#include "models/ComponentData.h"
#include "utils/PathSecurity.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>

namespace EasyKiConverter {

Model3DExportStage::Model3DExportStage(QObject* parent) : ExportTypeStage("Model3D", 2, parent) {}

Model3DExportStage::~Model3DExportStage() {
    cancel();
}

void Model3DExportStage::start(const QStringList& componentIds,
                               const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isExporting.load()) {
        qWarning() << "Model3DExportStage: Export already in progress";
        return;
    }

    m_threadPool.setMaxThreadCount(m_options.weakNetworkSupport ? 1 : 2);

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
    // 只有在需要导出对应类型时才创建临时文件
    const bool needWrl = m_options.needsModel3DWrl();
    const bool needStep = m_options.needsModel3DStep();

    qInfo() << "Model3DExportStage::start() - exportModel3DFormat:" << m_options.exportModel3DFormat
            << "needWrl:" << needWrl << "needStep:" << needStep;

    m_componentPaths.clear();
    for (const QString& componentId : componentIds) {
        TempFilePaths paths;
        // 最终文件名会在 startWorker() 中根据模型名称动态设置
        // 只有在用户选择对应格式时才创建临时路径
        if (needWrl) {
            paths.wrlTempPath =
                m_tempManager.createTempFilePath(componentId + QStringLiteral("_wrl"), QStringLiteral(".wrl"));
        }
        if (needStep) {
            paths.stepTempPath =
                m_tempManager.createTempFilePath(componentId + QStringLiteral("_step"), QStringLiteral(".step"));
        }

        if ((needWrl && !paths.wrlTempPath.isEmpty()) || (needStep && !paths.stepTempPath.isEmpty())) {
            m_componentPaths[componentId] = paths;
        } else if (needWrl || needStep) {
            // 用户请求了导出但临时路径创建失败，记录警告
            qWarning() << "Model3DExportStage: Failed to create temp path for component:" << componentId
                       << "needWrl:" << needWrl << "needStep:" << needStep;
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

    // 解析模型名称（优先使用3D模型名称，其次封装名称，最后回退到 componentId）
    QString modelName;
    if (data && data->model3DData()) {
        modelName = data->model3DData()->name();
    }
    if (modelName.isEmpty() && data && data->footprintData()) {
        modelName = data->footprintData()->info().name;
    }
    if (modelName.isEmpty()) {
        modelName = componentId;
    }
    modelName = PathSecurity::sanitizeFilename(modelName);

    // 使用模型名称设置最终输出路径
    const bool needWrl = m_options.needsModel3DWrl();
    const bool needStep = m_options.needsModel3DStep();
    QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
    QString baseOutputDir = m_options.outputPath;
    if (baseOutputDir.isEmpty()) {
        baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString outputDir = baseOutputDir + QDir::separator() + libName + QStringLiteral(".3dmodels");

    if (m_componentPaths.contains(componentId)) {
        TempFilePaths& paths = m_componentPaths[componentId];
        paths.wrlFinalPath = needWrl ? (outputDir + QDir::separator() + modelName + QStringLiteral(".wrl")) : QString();
        paths.stepFinalPath =
            needStep ? (outputDir + QDir::separator() + modelName + QStringLiteral(".step")) : QString();
        exportWorker->setOutputPaths({paths.wrlTempPath, paths.stepTempPath});
    }

    // 使用QPointer防止stage已销毁时lambda访问悬空指针
    QPointer<Model3DExportStage> stagePtr(this);
    qInfo() << "Model3DExportStage: Connecting worker completed signal for" << componentId;
    connect(
        exportWorker,
        &Model3DExportWorker::completed,
        this,
        [stagePtr, exportWorker, componentId](const QString&, bool success, const QString& error) {
            qInfo() << "Model3DExportStage: Worker completed signal received for" << componentId
                    << "success:" << success;
            if (!stagePtr) {
                qInfo() << "Model3DExportStage: stagePtr is null!";
                exportWorker->deleteLater();
                return;
            }

            // 如果成功，提交临时文件
            if (success && stagePtr->m_componentPaths.contains(componentId)) {
                const TempFilePaths paths = stagePtr->m_componentPaths.value(componentId);
                const bool needWrl = stagePtr->m_options.needsModel3DWrl();
                const bool needStep = stagePtr->m_options.needsModel3DStep();

                bool wrlCommitted = true;
                bool stepCommitted = true;
                if (needWrl) {
                    wrlCommitted = stagePtr->commitTempFile(paths.wrlTempPath, paths.wrlFinalPath);
                }
                if (needStep) {
                    stepCommitted = stagePtr->commitTempFile(paths.stepTempPath, paths.stepFinalPath);
                }
                if ((needWrl && !wrlCommitted) || (needStep && !stepCommitted)) {
                    success = false;
                }
            }

            stagePtr->completeItemProgress(exportWorker, componentId, success, error);
            exportWorker->deleteLater();
        },
        Qt::QueuedConnection);

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
