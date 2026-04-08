#include "Model3DExportWorker.h"

#include "../../core/kicad/Exporter3DModel.h"
#include "../../models/ComponentData.h"

#include <QDebug>
#include <QDir>
#include <QFile>

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

    QString fileName = m_componentId + QStringLiteral(".wrl");
    QString filePath = outputDir + QStringLiteral("/") + fileName;

    // 检查文件是否已存在
    if (QFile::exists(filePath) && !m_options.overwriteExistingFiles) {
        qDebug() << "Model3DExportWorker: File already exists, skipping" << filePath;
        emit completed(m_componentId, true, QString());
        return;
    }

    // 使用缓存的UUID通过Exporter3DModel下载3D模型
    QString uuid = m_data->model3DData()->uuid();

    // 连接Exporter3DModel的下载完成信号
    connect(
        m_exporter,
        &Exporter3DModel::downloadSuccess,
        this,
        [this](const QString& downloadedFilePath) {
            Q_UNUSED(downloadedFilePath);
            if (m_cancelled.load()) {
                emit completed(m_componentId, false, QStringLiteral("Cancelled"));
                return;
            }
            qDebug() << "Model3DExportWorker: Successfully downloaded 3D model for" << m_componentId;
            emit completed(m_componentId, true, QString());
        },
        Qt::SingleShotConnection);

    connect(
        m_exporter,
        &Exporter3DModel::downloadError,
        this,
        [this](const QString& errorMessage) { onDownloadError(errorMessage); },
        Qt::SingleShotConnection);

    // 启动下载
    m_exporter->downloadObjModel(uuid, filePath);
}

void Model3DExportWorker::onDownloadError(const QString& error) {
    qCritical() << "Model3DExportWorker: Download error:" << error;
    emit completed(m_componentId, false, error);
}

void Model3DExportWorker::cancel() {
    m_cancelled.store(true);
    if (m_exporter) {
        m_exporter->cancel();
    }
}

}  // namespace EasyKiConverter
