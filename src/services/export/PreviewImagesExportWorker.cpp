#include "PreviewImagesExportWorker.h"

#include "../../models/ComponentData.h"
#include "../../services/ComponentCacheService.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>

namespace EasyKiConverter {

PreviewImagesExportWorker::PreviewImagesExportWorker() : QObject(nullptr) {
    setAutoDelete(false);
}

void PreviewImagesExportWorker::setData(const QString& componentId,
                                        const QSharedPointer<ComponentData>& data,
                                        const struct ExportOptions& options) {
    m_componentId = componentId;
    m_data = data;
    m_options = options;
}

void PreviewImagesExportWorker::setOptions(const struct ExportOptions& options) {
    m_options = options;
}

void PreviewImagesExportWorker::run() {
    if (m_cancelled.load()) {
        emit completed(m_componentId, false, QStringLiteral("Cancelled"));
        return;
    }

    if (!m_data) {
        emit completed(m_componentId, false, QStringLiteral("No data available"));
        return;
    }

    qDebug() << "PreviewImagesExportWorker: Exporting" << m_componentId;

    // 检查预览图数据是否可用
    QList<QByteArray> previewDataList = m_data->previewImageData();

    // 如果内存中没有预览图数据，尝试从磁盘缓存加载
    if (previewDataList.isEmpty()) {
        ComponentCacheService* cache = ComponentCacheService::instance();
        for (int i = 0; i < 3; ++i) {  // 最多尝试加载3张预览图
            QByteArray imageData = cache->loadPreviewImage(m_componentId, i);
            if (imageData.isEmpty()) {
                break;
            }
            previewDataList.append(imageData);
        }
        qDebug() << "PreviewImagesExportWorker: Loaded" << previewDataList.size() << "preview images from cache for"
                 << m_componentId;
    }

    // 如果缓存被清空，但内存里仍保留了预览图 URL，则在导出阶段直接回补下载。
    if (previewDataList.isEmpty()) {
        const QStringList previewUrls = m_data->previewImages();
        if (!previewUrls.isEmpty()) {
            ComponentCacheService* cache = ComponentCacheService::instance();
            for (int i = 0; i < previewUrls.size(); ++i) {
                const QByteArray imageData = cache->downloadPreviewImage(
                    m_componentId, previewUrls[i], i, nullptr, nullptr, m_options.weakNetworkSupport);
                if (!imageData.isEmpty()) {
                    previewDataList.append(imageData);
                }
            }
            qDebug() << "PreviewImagesExportWorker: Re-downloaded" << previewDataList.size()
                     << "preview images from URLs for" << m_componentId;
        }
    }

    if (previewDataList.isEmpty()) {
        // 无预览图数据时按跳过处理，不阻塞整体导出
        emit completed(m_componentId, true, QStringLiteral("Preview image data not available, skipped"));
        return;
    }

    // 构建输出路径
    // 注意：如果没有使用临时路径（previewCount <= 0 的情况），
    // m_options.outputPath 仍然是根目录，需要手动添加 .preview 子目录
    QString outputDir;
    if (m_tempPaths.isEmpty()) {
        // 没有使用临时路径，使用 .preview 子目录
        QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
        QString baseOutputDir = m_options.outputPath;
        if (baseOutputDir.isEmpty()) {
            baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
        }
        outputDir = baseOutputDir + QDir::separator() + libName + QStringLiteral(".preview");
    } else {
        // 使用 setTempPaths 设置的路径
        outputDir = m_options.outputPath;
    }

    QDir dir;
    if (!dir.mkpath(outputDir)) {
        emit completed(m_componentId, false, QStringLiteral("Failed to create output directory"));
        return;
    }

    // 遍历导出所有预览图
    int successCount = 0;
    int totalCount = previewDataList.size();

    for (int i = 0; i < totalCount; ++i) {
        if (m_cancelled.load()) {
            emit completed(m_componentId, false, QStringLiteral("Cancelled"));
            return;
        }

        const auto& previewData = previewDataList[i];
        QString fileName = QStringLiteral("%1_preview_%2.png").arg(m_componentId).arg(i + 1);
        QString finalPath = outputDir + QStringLiteral("/") + fileName;

        // 使用temp路径如果可用
        QString filePath = m_tempPaths.contains(fileName) ? m_tempPaths[fileName] : finalPath;

        // 确保目标目录存在（temp目录按组件拆分，可能尚未创建）
        const QString targetDir = QFileInfo(filePath).absolutePath();
        if (!QDir().mkpath(targetDir)) {
            qWarning() << "PreviewImagesExportWorker: Failed to create target dir" << targetDir;
            continue;
        }

        // 检查文件是否已存在（仅对最终路径且非temp模式）
        if (filePath == finalPath && QFile::exists(finalPath) && !m_options.overwriteExistingFiles) {
            qDebug() << "PreviewImagesExportWorker: File already exists, skipping" << finalPath;
            successCount++;
            continue;
        }

        try {
            // 解码并保存预览图
            QImage image = QImage::fromData(previewData);
            if (image.isNull()) {
                qWarning() << "PreviewImagesExportWorker: Failed to decode preview image" << i;
                continue;
            }

            QSaveFile file(filePath);
            if (!file.open(QIODevice::WriteOnly)) {
                qWarning() << "PreviewImagesExportWorker: Failed to open file" << filePath << file.errorString();
                continue;
            }

            if (!image.save(&file, "PNG")) {
                qWarning() << "PreviewImagesExportWorker: Failed to save image" << filePath;
                continue;
            }

            if (!file.commit()) {
                qWarning() << "PreviewImagesExportWorker: Failed to commit file" << filePath;
                continue;
            }

            successCount++;

        } catch (const std::exception& e) {
            qWarning() << "PreviewImagesExportWorker: Exception during export:" << e.what();
        }
    }

    if (successCount == totalCount) {
        qDebug() << "PreviewImagesExportWorker: Successfully exported all previews for" << m_componentId;
        emit completed(m_componentId, true, QString());
    } else if (successCount > 0) {
        // 部分成功也视为失败，避免提交不完整的临时文件
        qWarning() << "PreviewImagesExportWorker: Partially exported" << successCount << "/" << totalCount
                   << "- treating as failure";
        emit completed(
            m_componentId,
            false,
            QStringLiteral("Partial export failure: %1 of %2 images exported").arg(successCount).arg(totalCount));
    } else {
        emit completed(m_componentId, false, QStringLiteral("Failed to export any preview images"));
    }
}

void PreviewImagesExportWorker::cancel() {
    m_cancelled.store(true);
}

}  // namespace EasyKiConverter
