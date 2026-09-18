#include "ComponentListClipboardCoordinator.h"

#include "ComponentListViewModel.h"

#include <QClipboard>
#include <QGuiApplication>

namespace EasyKiConverter {

/** @brief 从剪贴板文本提取编号、过滤重复项并发起批量添加。 */
void ComponentListClipboardCoordinator::paste(ComponentListViewModel& owner) {
    QClipboard* clipboard = QGuiApplication::clipboard();
    const QString text = clipboard->text();

    if (text.isEmpty()) {
        qWarning() << "Clipboard is empty";
        return;
    }

    const QStringList extractedIds = owner.extractComponentIdFromText(text);
    if (extractedIds.isEmpty()) {
        qWarning() << "No valid component IDs found in clipboard";
        emit owner.pasteCompleted(0, 0);
        return;
    }

    int skipped = 0;
    QStringList newIds;
    for (const QString& id : extractedIds) {
        if (owner.componentExists(id)) {
            skipped++;
        } else {
            newIds.append(id);
        }
    }

    if (!newIds.isEmpty()) {
        owner.addComponentsBatch(newIds);
    }

    emit owner.pasteCompleted(newIds.count(), skipped);
}

/** @brief 将列表中仍然有效的元件编号按换行格式复制到剪贴板。 */
void ComponentListClipboardCoordinator::copyAll(ComponentListViewModel& owner) {
    if (owner.m_componentList.isEmpty()) {
        qWarning() << "Component list is empty, nothing to copy";
        return;
    }

    QStringList componentIds;
    for (const auto* item : owner.m_componentList) {
        if (item) {
            componentIds.append(item->componentId());
        }
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(componentIds.join("\n"));

    qDebug() << "Copied" << componentIds.size() << "component IDs to clipboard";
}

}  // namespace EasyKiConverter
