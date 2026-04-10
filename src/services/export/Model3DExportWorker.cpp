#include "Model3DExportWorker.h"

#include "../../core/kicad/Exporter3DModel.h"
#include "../../models/ComponentData.h"

#include <QDebug>
#include <QDir>
#include <QEventLoop>
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
    QString finalPath = outputDir + QStringLiteral("/") + fileName;

    // 使用tempPath if available, otherwise use finalPath
    QString writePath = m_tempPath.isEmpty() ? finalPath : m_tempPath;

    // 检查文件是否已存在（仅对最终路径检查）
    if (QFile::exists(finalPath) && !m_options.overwriteExistingFiles && m_tempPath.isEmpty()) {
        qDebug() << "Model3DExportWorker: File already exists, skipping" << finalPath;
        emit completed(m_componentId, true, QString());
        return;
    }

    // 使用缓存的UUID通过Exporter3DModel下载3D模型
    QString uuid = m_data->model3DData()->uuid();

    // 创建事件循环来等待异步下载完成
    QEventLoop loop;
    QString error;

    // 连接Exporter3DModel的下载完成信号
    connect(
        m_exporter,
        &Exporter3DModel::downloadSuccess,
        &loop,
        [this, &loop, &error](const QString& downloadedFilePath) {
            Q_UNUSED(downloadedFilePath);
            if (m_cancelled.load()) {
                error = QStringLiteral("Cancelled");
            } else {
                qDebug() << "Model3DExportWorker: Successfully downloaded 3D model for" << m_componentId;
            }
            loop.quit();
        },
        Qt::SingleShotConnection);

    connect(
        m_exporter,
        &Exporter3DModel::downloadError,
        &loop,
        [this, &loop, &error](const QString& errorMessage) {
            error = errorMessage;
            qCritical() << "Model3DExportWorker: Download error:" << error;
            loop.quit();
        },
        Qt::SingleShotConnection);

    // 启动下载到临时路径
    m_exporter->downloadObjModel(uuid, writePath);

    // 等待下载完成或取消
    loop.exec();

    // 发送完成信号
    if (error.isEmpty()) {
        emit completed(m_componentId, true, QString());
    } else {
        emit completed(m_componentId, false, error);
    }
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
