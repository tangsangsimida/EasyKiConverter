#include "ExportTargetModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace EasyKiConverter {

/**
 * @brief 构造函数
 */
ExportTargetModel::ExportTargetModel(QObject* parent)
    : QObject(parent) {
}

int ExportTargetModel::currentIndex() const {
    return m_currentIndex;
}

void ExportTargetModel::setCurrentIndex(int index) {
    if (index < 0 || index >= m_targets.size()) {
        return;
    }
    if (m_currentIndex == index) {
        return;
    }
    m_currentIndex = index;
    emit currentTargetChanged();
}

QString ExportTargetModel::currentTargetId() const {
    if (m_currentIndex >= 0 && m_currentIndex < m_targets.size()) {
        return m_targets[m_currentIndex].id;
    }
    return QString();
}

QString ExportTargetModel::currentDisplayName() const {
    if (m_currentIndex >= 0 && m_currentIndex < m_targets.size()) {
        return m_targets[m_currentIndex].displayName;
    }
    return QString();
}

QString ExportTargetModel::currentIcon() const {
    if (m_currentIndex >= 0 && m_currentIndex < m_targets.size()) {
        return m_targets[m_currentIndex].icon;
    }
    return QString();
}

QString ExportTargetModel::currentOptionsComponent() const {
    if (m_currentIndex >= 0 && m_currentIndex < m_targets.size()) {
        return m_targets[m_currentIndex].optionsComponent;
    }
    return QString();
}

QVariantList ExportTargetModel::availableTargets() const {
    return m_availableTargetsCache;
}

/**
 * @brief 从 JSON 文件加载插件配置
 */
void ExportTargetModel::loadPlugins(const QString& configPath) {
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ExportTargetModel: Failed to open plugin config:" << configPath;
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "ExportTargetModel: Invalid JSON in plugin config:" << configPath;
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray plugins = root["plugins"].toArray();

    m_targets.clear();
    for (const QJsonValue& val : plugins) {
        QJsonObject obj = val.toObject();
        TargetInfo info;
        info.id = obj["id"].toString();
        info.displayName = obj["displayName"].toString();
        info.icon = obj["icon"].toString();
        info.optionsComponent = obj["optionsComponent"].toString();
        if (!info.id.isEmpty() && !info.displayName.isEmpty()) {
            m_targets.append(info);
        }
    }

    // 重建缓存
    m_availableTargetsCache.clear();
    for (const TargetInfo& info : m_targets) {
        QVariantMap map;
        map["id"] = info.id;
        map["displayName"] = info.displayName;
        map["icon"] = info.icon;
        map["optionsComponent"] = info.optionsComponent;
        m_availableTargetsCache.append(map);
    }

    // 确保索引有效
    bool indexChanged = false;
    if (m_currentIndex >= m_targets.size()) {
        m_currentIndex = 0;
        indexChanged = true;
    }

    emit availableTargetsChanged();
    if (indexChanged) {
        emit currentTargetChanged();
    }
}

}  // namespace EasyKiConverter
