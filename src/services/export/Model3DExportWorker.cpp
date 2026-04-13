#include "Model3DExportWorker.h"

#include "../../core/kicad/Exporter3DModel.h"
#include "../../models/ComponentData.h"

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

    // 检查3D模型UUID是否可用
    if (!m_data->model3DData() || m_data->model3DData()->uuid().isEmpty()) {
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

    if (!m_options.overwriteExistingFiles && m_outputPaths.wrlTempPath.isEmpty() && m_outputPaths.stepTempPath.isEmpty() &&
        QFile::exists(wrlFinalPath) && QFile::exists(stepFinalPath)) {
        qDebug() << "Model3DExportWorker: Files already exist, skipping" << wrlFinalPath << stepFinalPath;
        emit completed(m_componentId, true, QString());
        return;
    }

    QString uuid = m_data->model3DData()->uuid();

    auto downloadModel = [this, &uuid](auto downloadMethod, const QString& targetPath, const QString& formatName) {
        QString localError;
        bool downloadCompleted = false;

        QMetaObject::Connection successConnection;
        QMetaObject::Connection errorConnection;

        successConnection = connect(
            m_exporter,
            &Exporter3DModel::downloadSuccess,
            this,
            [this, &downloadCompleted, &localError, targetPath](const QString& downloadedFilePath) {
                if (downloadedFilePath != targetPath) {
                    return;
                }
                if (m_cancelled.load()) {
                    localError = QStringLiteral("Cancelled");
                    return;
                }
                downloadCompleted = true;
            });

        errorConnection = connect(
            m_exporter,
            &Exporter3DModel::downloadError,
            this,
            [&localError](const QString& errorMessage) { localError = errorMessage; });

        (m_exporter->*downloadMethod)(uuid, targetPath);

        disconnect(successConnection);
        disconnect(errorConnection);

        if (!downloadCompleted && localError.isEmpty()) {
            localError = QStringLiteral("%1 export did not complete").arg(formatName);
        }
        return localError;
    };

    QString error = downloadModel(&Exporter3DModel::downloadObjModel, wrlWritePath, QStringLiteral("WRL"));
    if (error.isEmpty() && !m_cancelled.load()) {
        error = downloadModel(&Exporter3DModel::downloadStepModel, stepWritePath, QStringLiteral("STEP"));
    }

    if (m_cancelled.load() && error.isEmpty()) {
        error = QStringLiteral("Cancelled");
    }

    if (error.isEmpty()) {
        qDebug() << "Model3DExportWorker: Successfully exported WRL and STEP for" << m_componentId;
        emit completed(m_componentId, true, QString());
        return;
    }

    qCritical("%s", qPrintable(QString("Model3DExportWorker: Download error: %1").arg(error)));
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
