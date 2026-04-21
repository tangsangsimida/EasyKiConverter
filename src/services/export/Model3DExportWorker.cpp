#include "Model3DExportWorker.h"

#include "../../core/kicad/Exporter3DModel.h"
#include "../../models/ComponentData.h"
#include "../../services/ComponentCacheService.h"
#include "../../utils/PathSecurity.h"
#include "DebugExportHelper.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

Model3DExportWorker::Model3DExportWorker(QObject* parent) : QObject(parent), m_exporter(new Exporter3DModel(this)) {
    setAutoDelete(false);
}

Model3DExportWorker::~Model3DExportWorker() {
    if (m_exporter) {
        m_exporter->cancel();
    }
}

void Model3DExportWorker::setData(const QString& componentId,
                                  const QSharedPointer<ComponentData>& data,
                                  const struct ExportOptions& options) {
    m_componentId = componentId;
    m_data = data;
    m_options = options;
}

void Model3DExportWorker::setOptions(const struct ExportOptions& options) {
    m_options = options;
}

void Model3DExportWorker::run() {
    if (m_cancelled.load()) {
        emit completed(m_componentId, false, QStringLiteral("Cancelled"));
        return;
    }

    if (!m_data) {
        emit completed(m_componentId, false, QStringLiteral("No data available"));
        return;
    }

    qDebug() << "Model3DExportWorker: Exporting" << m_componentId;

    QString uuid;
    if (m_data->model3DData()) {
        uuid = m_data->model3DData()->uuid();
    }
    if (uuid.isEmpty()) {
        QSharedPointer<ComponentData> cachedComponent =
            ComponentCacheService::instance()->loadComponentData(m_componentId);
        if (cachedComponent && cachedComponent->model3DData()) {
            uuid = cachedComponent->model3DData()->uuid();
        }
    }
    if (uuid.isEmpty()) {
        emit completed(m_componentId, false, QStringLiteral("3D model UUID not available"));
        return;
    }

    // 根据选项决定是否导出 WRL 和 STEP
    const bool needWrl = m_options.needsModel3DWrl();
    const bool needStep = m_options.needsModel3DStep();

    qInfo() << "Model3DExportWorker::run() for" << m_componentId
            << "exportModel3DFormat:" << m_options.exportModel3DFormat << "needWrl:" << needWrl
            << "needStep:" << needStep;

    // 如果两种格式都不需要导出，直接完成
    if (!needWrl && !needStep) {
        qDebug() << "Model3DExportWorker: No format selected, skipping" << m_componentId;
        emit completed(m_componentId, true, QString());
        return;
    }

    // 获取模型名称用于命名文件（仅在需要时）
    QString modelName;
    if (m_data && m_data->model3DData()) {
        modelName = m_data->model3DData()->name();
    }
    if (modelName.isEmpty()) {
        modelName = m_componentId;
    } else {
        // 使用 PathSecurity 清理文件名中的非法字符
        modelName = PathSecurity::sanitizeFilename(modelName);
    }

    // 构建输出路径
    QString outputDir = m_options.outputPath;
    if (outputDir.isEmpty()) {
        outputDir = QDir::currentPath() + QStringLiteral("/export/3dmodels");
    }

    QDir dir;
    if (!dir.mkpath(outputDir)) {
        emit completed(m_componentId, false, QStringLiteral("Failed to create output directory"));
        return;
    }

    const QString wrlFinalPath =
        needWrl ? (outputDir + QStringLiteral("/") + modelName + QStringLiteral(".wrl")) : QString();
    const QString stepFinalPath =
        needStep ? (outputDir + QStringLiteral("/") + modelName + QStringLiteral(".step")) : QString();
    const QString wrlWritePath = m_outputPaths.wrlTempPath.isEmpty() ? wrlFinalPath : m_outputPaths.wrlTempPath;
    const QString stepWritePath = m_outputPaths.stepTempPath.isEmpty() ? stepFinalPath : m_outputPaths.stepTempPath;

    const auto ensureParentDir = [](const QString& filePath) {
        if (filePath.isEmpty())
            return true;
        QFileInfo fileInfo(filePath);
        return fileInfo.absoluteDir().exists() || QDir().mkpath(fileInfo.absolutePath());
    };

    if ((needWrl && !ensureParentDir(wrlWritePath)) || (needStep && !ensureParentDir(stepWritePath))) {
        emit completed(m_componentId, false, QStringLiteral("Failed to create 3D model output directory"));
        return;
    }

    if (!m_options.overwriteExistingFiles && m_outputPaths.wrlTempPath.isEmpty() &&
        m_outputPaths.stepTempPath.isEmpty()) {
        bool bothExist = true;
        if (needWrl && !QFile::exists(wrlFinalPath))
            bothExist = false;
        if (needStep && !QFile::exists(stepFinalPath))
            bothExist = false;
        if (bothExist && (needWrl || needStep)) {
            qDebug() << "Model3DExportWorker: Files already exist, skipping" << wrlFinalPath << stepFinalPath;
            emit completed(m_componentId, true, QString());
            return;
        }
    }

    ComponentCacheService* cache = ComponentCacheService::instance();

    // 根据选项决定是否需要从缓存加载
    bool wrlFromCache = false;
    bool stepFromCache = false;

    // 只有需要导出时才从缓存加载
    if (needWrl && cache->hasModel3DCached(uuid, QStringLiteral("wrl")) &&
        cache->copyModel3DToFile(uuid, QStringLiteral("wrl"), wrlWritePath)) {
        qDebug() << "Model3DExportWorker: WRL cache hit for" << uuid;
        wrlFromCache = true;
    }

    if (needStep && cache->hasModel3DCached(uuid, QStringLiteral("step")) &&
        cache->copyModel3DToFile(uuid, QStringLiteral("step"), stepWritePath)) {
        qDebug() << "Model3DExportWorker: STEP cache hit for" << uuid;
        stepFromCache = true;
    }

    // 如果两个文件都从缓存加载成功，直接完成
    if ((!needWrl || wrlFromCache) && (!needStep || stepFromCache)) {
        qDebug() << "Model3DExportWorker: Both WRL and STEP loaded from cache for" << m_componentId;
        emit completed(m_componentId, true, QString());
        return;
    }

    auto buildModelData = [this, &uuid]() {
        Model3DData modelData;
        modelData.setUuid(uuid);
        if (m_data && m_data->model3DData()) {
            modelData.setName(m_data->model3DData()->name());
            modelData.setTranslation(m_data->model3DData()->translation());
            modelData.setRotation(m_data->model3DData()->rotation());
        }
        return modelData;
    };

    QString error;
    // 只在需要 WRL 且未从缓存加载时导出
    if (needWrl && !wrlFromCache) {
        QByteArray objData;
        if (!m_exporter->downloadObjDataSync(uuid, &objData, &error)) {
            if (error.isEmpty()) {
                error = QStringLiteral("Failed to download OBJ data for WRL export");
            }
        } else {
            Model3DData modelData = buildModelData();
            modelData.setRawObj(QString::fromUtf8(objData));
            if (!m_exporter->exportToWrl(modelData, wrlWritePath)) {
                error = QStringLiteral("Failed to convert OBJ to WRL");
            } else {
                QFile file(wrlWritePath);
                if (file.open(QIODevice::ReadOnly)) {
                    cache->saveModel3D(uuid, file.readAll(), QStringLiteral("wrl"));
                    qDebug() << "Model3DExportWorker: Saved WRL to cache for" << m_componentId << "uuid" << uuid;
                }
            }
        }
    }
    // 只在需要 STEP 且未从缓存加载时导出
    if (error.isEmpty() && needStep && !stepFromCache && !m_cancelled.load()) {
        QByteArray stepData;
        if (!m_exporter->downloadStepDataSync(uuid, &stepData, &error)) {
            if (error.isEmpty()) {
                error = QStringLiteral("Failed to download STEP data");
            }
        } else {
            Model3DData modelData = buildModelData();
            modelData.setStep(stepData);
            if (!m_exporter->exportToStep(modelData, stepWritePath)) {
                error = QStringLiteral("Failed to write STEP file");
            } else {
                cache->saveModel3D(uuid, stepData, QStringLiteral("step"));
                qDebug() << "Model3DExportWorker: Saved STEP to cache for" << m_componentId << "uuid" << uuid;
            }
        }
    }

    if (m_cancelled.load() && error.isEmpty()) {
        error = QStringLiteral("Cancelled");
    }

    if (error.isEmpty()) {
        qDebug() << "Model3DExportWorker: Successfully exported" << (needWrl ? "WRL" : "") << (needStep ? "STEP" : "")
                 << "for" << m_componentId;

        // Debug 模式导出原始数据
        if (m_options.debugMode) {
            DebugExportHelper::exportDebugData(m_componentId, m_data, m_options.outputPath);
        }

        emit completed(m_componentId, true, QString());
        return;
    }

    qCritical() << "Model3DExportWorker: Export failed for" << m_componentId << "uuid" << uuid << "error:" << error;
    emit completed(m_componentId, false, error);
}

void Model3DExportWorker::onDownloadError(const QString& error) {
    qCritical("%s", qPrintable(QString("Model3DExportWorker: Download error: %1").arg(error)));
    emit completed(m_componentId, false, error);
}

void Model3DExportWorker::cancel() {
    m_cancelled.store(true);
    if (m_exporter) {
        m_exporter->cancel();
    }
}

}  // namespace EasyKiConverter
