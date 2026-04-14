#include "Model3DExportWorker.h"

#include "../../core/kicad/Exporter3DModel.h"
#include "../../models/ComponentData.h"
#include "../../services/ComponentCacheService.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

Model3DExportWorker::Model3DExportWorker(QObject* parent) : QObject(parent), m_exporter(new Exporter3DModel(this)) {}

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

    const QString wrlFinalPath = outputDir + QStringLiteral("/") + m_componentId + QStringLiteral(".wrl");
    const QString stepFinalPath = outputDir + QStringLiteral("/") + m_componentId + QStringLiteral(".step");
    const QString wrlWritePath = m_outputPaths.wrlTempPath.isEmpty() ? wrlFinalPath : m_outputPaths.wrlTempPath;
    const QString stepWritePath = m_outputPaths.stepTempPath.isEmpty() ? stepFinalPath : m_outputPaths.stepTempPath;

    const auto ensureParentDir = [](const QString& filePath) {
        QFileInfo fileInfo(filePath);
        return fileInfo.absoluteDir().exists() || QDir().mkpath(fileInfo.absolutePath());
    };

    if (!ensureParentDir(wrlWritePath) || !ensureParentDir(stepWritePath)) {
        emit completed(m_componentId, false, QStringLiteral("Failed to create 3D model output directory"));
        return;
    }

    if (!m_options.overwriteExistingFiles && m_outputPaths.wrlTempPath.isEmpty() &&
        m_outputPaths.stepTempPath.isEmpty() && QFile::exists(wrlFinalPath) && QFile::exists(stepFinalPath)) {
        qDebug() << "Model3DExportWorker: Files already exist, skipping" << wrlFinalPath << stepFinalPath;
        emit completed(m_componentId, true, QString());
        return;
    }

    ComponentCacheService* cache = ComponentCacheService::instance();

    // 检查缓存中是否已有3D模型文件
    bool wrlFromCache = false;
    bool stepFromCache = false;

    if (cache->hasModel3DCached(uuid, QStringLiteral("wrl")) &&
        cache->copyModel3DToFile(uuid, QStringLiteral("wrl"), wrlWritePath)) {
        qDebug() << "Model3DExportWorker: WRL cache hit for" << uuid;
        wrlFromCache = true;
    }

    if (cache->hasModel3DCached(uuid, QStringLiteral("step")) &&
        cache->copyModel3DToFile(uuid, QStringLiteral("step"), stepWritePath)) {
        qDebug() << "Model3DExportWorker: STEP cache hit for" << uuid;
        stepFromCache = true;
    }

    // 如果两个文件都从缓存加载成功，直接完成
    if (wrlFromCache && stepFromCache) {
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
    if (!wrlFromCache) {
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
    if (error.isEmpty() && !stepFromCache && !m_cancelled.load()) {
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
        qDebug() << "Model3DExportWorker: Successfully exported WRL and STEP for" << m_componentId;
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
