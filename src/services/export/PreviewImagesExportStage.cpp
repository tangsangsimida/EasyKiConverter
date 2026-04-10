#include "PreviewImagesExportStage.h"

#include "PreviewImagesExportWorker.h"
#include "models/ComponentData.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPointer>

namespace EasyKiConverter {

PreviewImagesExportStage::PreviewImagesExportStage(QObject* parent)
    : ExportTypeStage("PreviewImages", 4, parent) {
}

PreviewImagesExportStage::~PreviewImagesExportStage() {
    cancel();
}

void PreviewImagesExportStage::start(const QStringList& componentIds,
                                   const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isExporting.load()) {
        qWarning() << "PreviewImagesExportStage: Export already in progress";
        return;
    }

    // 构建输出目录：outputPath/libName.preview/
    QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
    QString baseOutputDir = m_options.outputPath;
    if (baseOutputDir.isEmpty()) {
        baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString outputDir = baseOutputDir + QDir::separator() + libName + QStringLiteral(".preview");

    // 确保输出目录存在
    QDir dir;
    if (!dir.mkpath(outputDir)) {
        qCritical() << "PreviewImagesExportStage: Failed to create output directory:" << outputDir;
        m_isExporting.store(false);
        // 不直接发送completed，让基类处理
        ExportTypeStage::start(QStringList(), cachedData);
        return;
    }

    // 设置临时文件管理器
    m_tempManager.setOutputPath(outputDir);

    // 为每个组件创建临时目录
    m_outputDirs.clear();
    for (const QString& componentId : componentIds) {
        m_outputDirs[componentId] = outputDir;
    }

    // 调用基类的初始化方法（设置状态，并处理空列表情况）
    ExportTypeStage::start(componentIds, cachedData);

    m_isExporting.store(true);
}

void PreviewImagesExportStage::cancel() {
    if (!m_isExporting.load()) {
        return;
    }

    qDebug() << "PreviewImagesExportStage: Cancelling...";

    // 设置取消标志
    m_cancelled.store(true);

    // 回滚所有临时文件
    m_tempManager.rollbackAll();

    m_isExporting.store(false);
    m_isRunning.store(false);

    qDebug() << "PreviewImagesExportStage: Cancelled";
}

QMap<QString, QString> PreviewImagesExportStage::createTempPathsForComponent(const QString& componentId,
                                                                              int previewCount) {
    QMap<QString, QString> paths;

    if (previewCount <= 0) {
        qWarning() << "PreviewImagesExportStage: No preview images to export for" << componentId;
        return paths;
    }

    QString outputDir = m_outputDirs.value(componentId);
    if (outputDir.isEmpty()) {
        qWarning() << "PreviewImagesExportStage: No output directory for" << componentId;
        return paths;
    }

    // 构造临时目录路径
    QString tempDir = outputDir + QDir::separator() + QStringLiteral(".tmp.") + componentId;

    for (int i = 1; i <= previewCount; ++i) {
        QString fileName = QStringLiteral("%1_preview_%2.png").arg(componentId).arg(i);
        QString tempPath = tempDir + QDir::separator() + fileName;
        paths[fileName] = tempPath;
    }

    if (paths.size() != previewCount) {
        qWarning() << "PreviewImagesExportStage: Created" << paths.size() << "paths but expected" << previewCount;
    }

    return paths;
}

QObject* PreviewImagesExportStage::createWorker() {
    return new PreviewImagesExportWorker();
}

void PreviewImagesExportStage::startWorker(QObject* worker,
                                          const QString& componentId,
                                          const QSharedPointer<ComponentData>& data) {
    auto* exportWorker = qobject_cast<PreviewImagesExportWorker*>(worker);
    if (!exportWorker) {
        qWarning() << "PreviewImagesExportStage: Failed to cast worker to PreviewImagesExportWorker";
        return;
    }

    exportWorker->setOptions(m_options);
    exportWorker->setData(componentId, data, m_options);

    // 为组件创建临时路径映射
    int previewCount = data->previewImageData().size();
    QMap<QString, QString> tempPaths = createTempPathsForComponent(componentId, previewCount);
    if (!tempPaths.isEmpty()) {
        exportWorker->setTempPaths(tempPaths);
    } else if (previewCount > 0) {
        // 临时路径创建失败但有预览图需要导出，记录警告
        qWarning() << "PreviewImagesExportStage: Temp path creation failed for" << componentId
                   << "- will use direct write";
    }

    // 使用QPointer防止stage已销毁时lambda访问悬空指针
    QPointer<PreviewImagesExportStage> stagePtr(this);
    connect(exportWorker,
            &PreviewImagesExportWorker::completed,
            this,
            [stagePtr, exportWorker, componentId](const QString&, bool success, const QString& error) {
                if (!stagePtr) {
                    return;
                }

                // 如果成功，提交所有临时文件
                if (success) {
                    QString outputDir = stagePtr->m_outputDirs.value(componentId);
                    QString tempDir = outputDir + QDir::separator() + QStringLiteral(".tmp.") + componentId;

                    // 移动所有临时文件到最终目录
                    QDir tempQDir(tempDir);
                    if (tempQDir.exists()) {
                        for (const QFileInfo& info : tempQDir.entryInfoList(QDir::Files)) {
                            QString fileName = info.fileName();
                            QString finalPath = outputDir + QDir::separator() + fileName;
                            QString tempPath = info.absoluteFilePath();

                            // 使用QFile::rename移动文件
                            if (QFile::exists(finalPath)) {
                                QFile::remove(finalPath);
                            }
                            if (QFile::rename(tempPath, finalPath)) {
                                qDebug() << "PreviewImagesExportStage: Committed" << finalPath;
                            }
                        }
                        // 删除临时目录
                        tempQDir.removeRecursively();
                    }
                }

                stagePtr->completeItemProgress(exportWorker, componentId, success, error);
            });

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
