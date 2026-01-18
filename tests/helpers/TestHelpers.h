#ifndef TESTHELPERS_H
#define TESTHELPERS_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDebug>
#include <QDir>

namespace TestHelpers {

/**
 * @brief 从 fixtures 目录加载 JSON 测试数据
 *
 * @param fileName JSON 文件名（相对于 fixtures 目录）
 * @return QJsonObject JSON 对象
 *
 * @note FIXTURES_DIR 需要在 CMake 中定义
 */
inline QJsonObject loadJsonFixture(const QString &fileName) {
    QString path = QString(FIXTURES_DIR) + "/" + fileName;
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("Cannot open fixture file: %s", qUtf8Printable(path));
        return QJsonObject();
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject()) {
        qFatal("Invalid JSON in fixture file: %s", qUtf8Printable(path));
        return QJsonObject();
    }

    return doc.object();
}

/**
 * @brief 从 fixtures 目录加载二进制测试数据
 *
 * @param fileName 文件名（相对于 fixtures 目录）
 * @return QByteArray 二进制数据
 */
inline QByteArray loadBinaryFixture(const QString &fileName) {
    QString path = QString(FIXTURES_DIR) + "/" + fileName;
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("Cannot open fixture file: %s", qUtf8Printable(path));
        return QByteArray();
    }

    return file.readAll();
}

/**
 * @brief 创建临时目录
 *
 * @param prefix 目录名前缀
 * @return QString 临时目录路径
 */
inline QString createTempDir(const QString &prefix = "test_") {
    QString tempPath = QDir::tempPath() + "/" + prefix + QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir dir;
    if (!dir.mkpath(tempPath)) {
        qFatal("Cannot create temp directory: %s", qUtf8Printable(tempPath));
        return QString();
    }
    return tempPath;
}

/**
 * @brief 清理临时目录
 *
 * @param path 目录路径
 */
inline void cleanupTempDir(const QString &path) {
    QDir dir(path);
    if (dir.exists()) {
        dir.removeRecursively();
    }
}

/**
 * @brief 创建简单的组件信息 JSON
 *
 * @param uuid 组件 UUID
 * @param name 组件名称
 * @param lcscId LCSC ID
 * @return QJsonObject 组件信息 JSON
 */
inline QJsonObject createSimpleComponentInfo(const QString &uuid, const QString &name, const QString &lcscId) {
    QJsonObject info;
    info["uuid"] = uuid;
    info["name"] = name;
    info["title"] = name;
    info["lcsc_id"] = lcscId;
    info["package"] = "SOP-8";
    info["package_type"] = "smd";
    return info;
}

/**
 * @brief 创建简单的 CAD 数据 JSON
 *
 * @param uuid 组件 UUID
 * @param footprintData 封装数据
 * @return QJsonObject CAD 数据 JSON
 */
inline QJsonObject createSimpleCadData(const QString &uuid, const QJsonObject &footprintData) {
    QJsonObject cadData;
    cadData["uuid"] = uuid;
    cadData["dataStr"] = footprintData;
    cadData["head"] = QJsonObject();
    cadData["head"].toObject()["uuid_3d"] = "test-3d-uuid";
    return cadData;
}

/**
 * @brief 比较两个 JSON 对象是否相等
 *
 * @param obj1 第一个 JSON 对象
 * @param obj2 第二个 JSON 对象
 * @return bool 是否相等
 */
inline bool jsonEquals(const QJsonObject &obj1, const QJsonObject &obj2) {
    QJsonDocument doc1(obj1);
    QJsonDocument doc2(obj2);
    return doc1.toJson(QJsonDocument::Compact) == doc2.toJson(QJsonDocument::Compact);
}

/**
 * @brief 打印 JSON 对象（用于调试）
 *
 * @param obj JSON 对象
 * @param label 标签
 */
inline void printJson(const QJsonObject &obj, const QString &label = "JSON") {
    QJsonDocument doc(obj);
    qDebug() << label << ":" << doc.toJson(QJsonDocument::Indented);
}

/**
 * @brief 等待信号（带超时）
 *
 * @param spy 信号监视器
 * @param timeoutMs 超时时间（毫秒）
 * @return bool 是否在超时前收到信号
 */
inline bool waitForSignal(QSignalSpy &spy, int timeoutMs = 5000) {
    return spy.wait(timeoutMs);
}

/**
 * @brief 等待多个信号（带超时）
 *
 * @param spies 信号监视器列表
 * @param count 每个监视器期望的信号数量
 * @param timeoutMs 超时时间（毫秒）
 * @return bool 所有监视器是否都收到了期望数量的信号
 */
inline bool waitForMultipleSignals(QList<QSignalSpy*> spies, int count, int timeoutMs = 5000) {
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        bool allReady = true;
        for (QSignalSpy *spy : spies) {
            if (spy->count() < count) {
                allReady = false;
                break;
            }
        }
        if (allReady) {
            return true;
        }
        QCoreApplication::processEvents();
        QTest::qWait(10);
    }
    return false;
}

} // namespace TestHelpers

#endif // TESTHELPERS_H