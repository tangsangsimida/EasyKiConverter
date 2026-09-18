#include "ComponentListPreviewCoordinator.h"

#include "ComponentListViewModel.h"

#include <QDebug>

namespace EasyKiConverter {

/** @brief 将缓存缓冲区中的完整和增量预览图更新应用到对应列表项。 */
void ComponentListPreviewCoordinator::processCacheImages(ComponentListViewModel& owner) {
    const ComponentListPreviewUpdateBuffer::Snapshot snapshot = owner.m_previewUpdateBuffer.take();
    const auto& pending = snapshot.completeImages;
    const auto& incrementalPending = snapshot.incrementalImages;

    if (pending.isEmpty() && incrementalPending.isEmpty()) {
        return;
    }

    for (auto it = incrementalPending.cbegin(); it != incrementalPending.cend(); ++it) {
        auto item = owner.findItemData(it.key());
        if (!item) {
            continue;
        }

        bool changed = false;
        for (auto imageIt = it.value().cbegin(); imageIt != it.value().cend(); ++imageIt) {
            item->setEncodedPreviewImageAt(imageIt.value(), imageIt.key(), false);
            changed = true;
        }

        if (changed) {
            item->notifyPreviewImagesChanged();
        }
    }

    for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
        auto item = owner.findItemData(it.key());
        if (item) {
            item->setEncodedPreviewImages(it.value());
            if (item->validationPhase() == "fetching_preview") {
                item->setValidationPhase("completed");
            }
        }
    }

    owner.scheduleListUpdate();
}

/** @brief 筛选有效且未重复的元件，并提交批量预览图请求。 */
void ComponentListPreviewCoordinator::fetchImages(ComponentListViewModel& owner, const QStringList& componentIds) {
    QStringList validIds;
    QSet<QString> seenIds;
    for (const QString& componentId : componentIds) {
        if (componentId.isEmpty() || seenIds.contains(componentId)) {
            continue;
        }
        seenIds.insert(componentId);

        auto item = owner.findItemData(componentId);
        if (item && item->isValid()) {
            validIds.append(componentId);
        }
    }

    if (validIds.isEmpty()) {
        qDebug() << "No valid components to fetch preview images for";
        owner.m_previewReadyHint = false;
        owner.m_validationReadyHint = false;
        emit owner.attentionStateChanged();
        return;
    }

    qDebug() << "Fetching preview images for" << validIds.count() << "valid components";
    owner.m_pendingPreviewFetchIds = QSet<QString>(validIds.begin(), validIds.end());
    owner.m_previewReadyHint = false;
    emit owner.attentionStateChanged();

    for (const QString& componentId : std::as_const(validIds)) {
        auto item = owner.findItemData(componentId);
        if (item && item->isValid() && item->previewImageCount() == 0) {
            item->setValidationPhase("fetching_preview");
        }
    }
    owner.scheduleListUpdate();

    // 使用批量获取预览图接口，避免为每个组件创建单独的定时器。
    // LcscImageService 会自动处理缓存加载和队列管理。
    owner.m_service->fetchBatchPreviewImages(validIds);
}

/** @brief 移除已完成元件并在全部完成后显示预览图提示。 */
void ComponentListPreviewCoordinator::markFetchCompleted(ComponentListViewModel& owner, const QString& componentId) {
    if (componentId.isEmpty() || !owner.m_pendingPreviewFetchIds.contains(componentId)) {
        return;
    }

    owner.m_pendingPreviewFetchIds.remove(componentId);
    if (owner.m_pendingPreviewFetchIds.isEmpty()) {
        owner.m_validationReadyHint = false;
        owner.m_previewReadyHint = true;
        emit owner.attentionStateChanged();
    }
}

/** @brief 清空预览图请求集合及验证、预览完成提示。 */
void ComponentListPreviewCoordinator::clearAttentionHints(ComponentListViewModel& owner) {
    const bool changed =
        owner.m_validationReadyHint || owner.m_previewReadyHint || !owner.m_pendingPreviewFetchIds.isEmpty();
    owner.m_validationReadyHint = false;
    owner.m_previewReadyHint = false;
    owner.m_pendingPreviewFetchIds.clear();
    if (changed) {
        emit owner.attentionStateChanged();
    }
}

}  // namespace EasyKiConverter
