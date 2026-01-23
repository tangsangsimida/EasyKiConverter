#ifndef COMPONENTDATACOLLECTOR_H
#define COMPONENTDATACOLLECTOR_H

#include <QObject>
#include <QString>
#include "models/ComponentData.h"
#include "models/SymbolData.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"

namespace EasyKiConverter
{

    /**
     * @brief 元件数据收集器类
     *
     * 使用状态机模式实现异步数据收集
     * 移除 QEventLoop 阻塞，提高并发性能
     */
    class ComponentDataCollector : public QObject
    {
        Q_OBJECT

    public:
        /**
         * @brief 收集状态枚�?
         */
        enum State
        {
            Idle,                  // 空闲状�?
            FetchingComponentInfo, // 正在获取组件信息
            FetchingCadData,       // 正在获取CAD数据
            FetchingObjData,       // 正在获取OBJ数据
            FetchingStepData,      // 正在获取STEP数据
            Completed,             // 完成
            Failed                 // 失败
        };
        Q_ENUM(State)

        /**
         * @brief 构造函�?
         *
         * @param componentId 元件ID
         * @param parent 父对�?
         */
        explicit ComponentDataCollector(const QString &componentId, QObject *parent = nullptr);

        /**
         * @brief 析构函数
         */
        ~ComponentDataCollector() override;

        /**
         * @brief 开始数据收�?
         */
        void start();

        /**
         * @brief 取消数据收集
         */
        void cancel();

        /**
         * @brief 获取当前状�?
         *
         * @return State 当前状�?
         */
        State state() const { return m_state; }

        /**
         * @brief 获取元件ID
         *
         * @return QString 元件ID
         */
        QString componentId() const { return m_componentId; }

        /**
         * @brief 获取收集到的元件数据
         *
         * @return ComponentData 元件数据
         */
        ComponentData componentData() const { return m_componentData; }

        /**
         * @brief 获取错误信息
         *
         * @return QString 错误信息
         */
        QString errorMessage() const { return m_errorMessage; }

        /**
         * @brief 设置是否需要导�?D模型
         *
         * @param export3D 是否导出3D模型
         */
        void setExport3DModel(bool export3D) { m_export3DModel = export3D; }

    signals:
        /**
         * @brief 状态改变信�?
         *
         * @param state 新状�?
         */
        void stateChanged(State state);

        /**
         * @brief 数据收集完成信号
         *
         * @param componentId 元件ID
         * @param data 元件数据
         */
        void dataCollected(const QString &componentId, const ComponentData &data);

        /**
         * @brief 错误发生信号
         *
         * @param componentId 元件ID
         * @param error 错误信息
         */
        void errorOccurred(const QString &componentId, const QString &error);

    private slots:
        /**
         * @brief 处理组件信息获取成功
         *
         * @param data 组件信息数据
         */
        void handleComponentInfoFetched(const QJsonObject &data);

        /**
         * @brief 处理CAD数据获取成功
         *
         * @param data CAD数据
         */
        void handleCadDataFetched(const QJsonObject &data);

        /**
         * @brief 处理3D模型数据获取成功
         *
         * @param uuid 模型UUID
         * @param data 3D模型数据
         */
        void handleModel3DFetched(const QString &uuid, const QByteArray &data);

        /**
         * @brief 处理获取错误
         *
         * @param errorMessage 错误信息
         */
        void handleFetchError(const QString &errorMessage);

    private:
        /**
         * @brief 设置状�?
         *
         * @param state 新状�?
         */
        void setState(State state);

        /**
         * @brief 完成数据收集
         */
        void complete();

        /**
         * @brief 处理错误
         *
         * @param error 错误信息
         */
        void handleError(const QString &error);

        /**
         * @brief 初始化API连接
         */
        void initializeApiConnections();

    private:
        QString m_componentId;
        State m_state;
        ComponentData m_componentData;
        QString m_errorMessage;
        bool m_export3DModel;
        bool m_isCancelled;

        // API和导入器
        class EasyedaApi *m_api;
        class EasyedaImporter *m_importer;
    };

} // namespace EasyKiConverter

#endif // COMPONENTDATACOLLECTOR_H
