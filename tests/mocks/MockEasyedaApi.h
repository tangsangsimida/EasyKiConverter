#ifndef MOCKEASYEDAAPI_H
#define MOCKEASYEDAAPI_H

#include "src/core/interfaces/IEasyedaApi.h"
#include <QJsonObject>
#include <QByteArray>
#include <QList>

namespace EasyKiConverter {

/**
 * @brief EasyedaApi 的 Mock 实现
 *
 * 用于单元测试，支持设置返回值和错误模式
 * 记录所有调用历史以便验证
 */
class MockEasyedaApi : public IEasyedaApi
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     *
     * @param parent 父对象
     */
    explicit MockEasyedaApi(QObject *parent = nullptr) : IEasyedaApi(parent) {}

    /**
     * @brief 析构函数
     */
    ~MockEasyedaApi() override = default;

    // ========== IEasyedaApi 接口实现 ==========

    void fetchComponentInfo(const QString &lcscId) override {
        m_fetchComponentInfo_calls.append(lcscId);
        m_lastLcscId = lcscId;

        if (m_errorMode) {
            emit fetchError(m_errorMessage);
            return;
        }

        if (m_delayMode) {
            QTimer::singleShot(m_delayMs, this, [this]() {
                emit componentInfoFetched(m_mockComponentInfo);
            });
        } else {
            emit componentInfoFetched(m_mockComponentInfo);
        }
    }

    void fetchCadData(const QString &lcscId) override {
        m_fetchCadData_calls.append(lcscId);

        if (m_errorMode) {
            emit fetchError(m_errorMessage);
            return;
        }

        if (m_delayMode) {
            QTimer::singleShot(m_delayMs, this, [this]() {
                emit cadDataFetched(m_mockCadData);
            });
        } else {
            emit cadDataFetched(m_mockCadData);
        }
    }

    void fetch3DModelObj(const QString &uuid) override {
        m_fetch3DModelObj_calls.append(uuid);

        if (m_errorMode) {
            emit fetchError(m_errorMessage);
            return;
        }

        if (m_delayMode) {
            QTimer::singleShot(m_delayMs, this, [this, uuid]() {
                emit model3DFetched(uuid, m_mock3DModelData);
            });
        } else {
            emit model3DFetched(uuid, m_mock3DModelData);
        }
    }

    void fetch3DModelStep(const QString &uuid) override {
        m_fetch3DModelStep_calls.append(uuid);

        if (m_errorMode) {
            emit fetchError(m_errorMessage);
            return;
        }

        if (m_delayMode) {
            QTimer::singleShot(m_delayMs, this, [this, uuid]() {
                emit model3DFetched(uuid, m_mock3DModelData);
            });
        } else {
            emit model3DFetched(uuid, m_mock3DModelData);
        }
    }

    void cancelRequest() override {
        m_cancelRequest_calls.append(true);
    }

    // ========== Mock 设置方法 ==========

    /**
     * @brief 设置 Mock 组件信息
     *
     * @param info 组件信息 JSON
     */
    void setMockComponentInfo(const QJsonObject &info) {
        m_mockComponentInfo = info;
    }

    /**
     * @brief 设置 Mock CAD 数据
     *
     * @param data CAD 数据 JSON
     */
    void setMockCadData(const QJsonObject &data) {
        m_mockCadData = data;
    }

    /**
     * @brief 设置 Mock 3D 模型数据
     *
     * @param data 3D 模型数据
     */
    void setMock3DModelData(const QByteArray &data) {
        m_mock3DModelData = data;
    }

    /**
     * @brief 设置错误模式
     *
     * @param error 是否启用错误模式
     * @param errorMessage 错误消息
     */
    void setErrorMode(bool error, const QString &errorMessage = "Mock error") {
        m_errorMode = error;
        m_errorMessage = errorMessage;
    }

    /**
     * @brief 设置延迟模式（模拟网络延迟）
     *
     * @param delay 延迟时间（毫秒）
     */
    void setDelayMode(int delayMs) {
        m_delayMode = true;
        m_delayMs = delayMs;
    }

    /**
     * @brief 清除延迟模式
     */
    void clearDelayMode() {
        m_delayMode = false;
        m_delayMs = 0;
    }

    // ========== 调用历史查询方法 ==========

    /**
     * @brief 获取 fetchComponentInfo 调用历史
     *
     * @return QList<QString> 调用参数列表
     */
    QList<QString> getFetchComponentInfoCalls() const {
        return m_fetchComponentInfo_calls;
    }

    /**
     * @brief 获取 fetchCadData 调用历史
     *
     * @return QList<QString> 调用参数列表
     */
    QList<QString> getFetchCadDataCalls() const {
        return m_fetchCadData_calls;
    }

    /**
     * @brief 获取 fetch3DModelObj 调用历史
     *
     * @return QList<QString> 调用参数列表
     */
    QList<QString> getFetch3DModelObjCalls() const {
        return m_fetch3DModelObj_calls;
    }

    /**
     * @brief 获取 fetch3DModelStep 调用历史
     *
     * @return QList<QString> 调用参数列表
     */
    QList<QString> getFetch3DModelStepCalls() const {
        return m_fetch3DModelStep_calls;
    }

    /**
     * @brief 获取 cancelRequest 调用次数
     *
     * @return int 调用次数
     */
    int getCancelRequestCalls() const {
        return m_cancelRequest_calls.size();
    }

    /**
     * @brief 清除所有调用历史
     */
    void clearCallHistory() {
        m_fetchComponentInfo_calls.clear();
        m_fetchCadData_calls.clear();
        m_fetch3DModelObj_calls.clear();
        m_fetch3DModelStep_calls.clear();
        m_cancelRequest_calls.clear();
    }

private:
    // Mock 数据
    QJsonObject m_mockComponentInfo;
    QJsonObject m_mockCadData;
    QByteArray m_mock3DModelData;

    // Mock 行为控制
    bool m_errorMode = false;
    QString m_errorMessage;
    bool m_delayMode = false;
    int m_delayMs = 0;

    // 调用历史
    QList<QString> m_fetchComponentInfo_calls;
    QList<QString> m_fetchCadData_calls;
    QList<QString> m_fetch3DModelObj_calls;
    QList<QString> m_fetch3DModelStep_calls;
    QList<bool> m_cancelRequest_calls;

    // 最后调用的参数
    QString m_lastLcscId;
};

} // namespace EasyKiConverter

#endif // MOCKEASYEDAAPI_H