#ifndef EXPORTER3DMODEL_H
#define EXPORTER3DMODEL_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include "models/Model3DData.h"

// 前向声明
namespace EasyKiConverter
{
    class NetworkUtils;
}

namespace EasyKiConverter
{

    /**
     * @brief 3D 模型导出器类
     *
     * 用于处理 EasyEDA 3D 模型数据的下载和转换
     */
    class Exporter3DModel : public QObject
    {
        Q_OBJECT

    public:
        /**
         * @brief 3D 模型格式枚举
         */
        enum class ModelFormat
        {
            OBJ, // Wavefront OBJ 格式
            STEP // STEP 格式
        };
        Q_ENUM(ModelFormat)

        /**
         * @brief 构造函�?
         *
         * @param parent 父对�?
         */
        explicit Exporter3DModel(QObject *parent = nullptr);

        /**
         * @brief 析构函数
         */
        ~Exporter3DModel() override;

        /**
         * @brief 下载 3D 模型（OBJ 格式�?
         *
         * @param uuid 模型 UUID
         * @param savePath 保存路径
         */
        void downloadObjModel(const QString &uuid, const QString &savePath);

        /**
         * @brief 下载 3D 模型（STEP 格式�?
         *
         * @param uuid 模型 UUID
         * @param savePath 保存路径
         */
        void downloadStepModel(const QString &uuid, const QString &savePath);

        /**
         * @brief 导出模型�?KiCad WRL 格式
         *
         * @param modelData 模型数据
         * @param savePath 保存路径
         * @return bool 是否成功
         */
        bool exportToWrl(const Model3DData &modelData, const QString &savePath);

        /**
         * @brief 导出模型�?STEP 格式
         *
         * @param modelData 模型数据
         * @param savePath 保存路径
         * @return bool 是否成功
         */
        bool exportToStep(const Model3DData &modelData, const QString &savePath);

        /**
         * @brief �?3D 模型转换�?KiCad 坐标�?
         *
         * @param modelData 模型数据
         */
        void convertToKiCadCoordinates(Model3DData &modelData);

        /**
         * @brief 取消下载
         */
        void cancel();

    signals:
        /**
         * @brief 下载成功信号
         *
         * @param filePath 保存的文件路�?
         */
        void downloadSuccess(const QString &filePath);

        /**
         * @brief 下载失败信号
         *
         * @param errorMessage 错误信息
         */
        void downloadError(const QString &errorMessage);

        /**
         * @brief 下载进度信号
         *
         * @param bytesReceived 已接收的字节�?
         * @param bytesTotal 总字节数
         */
        void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    private:
        /**
         * @brief 生成 WRL 文件内容
         *
         * @param modelData 模型数据
         * @param objData OBJ 模型数据
         * @return QString WRL 文件内容
         */
        QString generateWrlContent(const Model3DData &modelData, const QByteArray &objData);

        /**
         * @brief 解析 OBJ 数据
         *
         * @param objData OBJ 模型数据
         * @return QJsonObject 解析后的数据
         */
        QJsonObject parseObjData(const QByteArray &objData);

        /**
         * @brief 获取模型 URL
         *
         * @param uuid 模型 UUID
         * @param format 模型格式
         * @return QString 模型 URL
         */
        QString getModelUrl(const QString &uuid, ModelFormat format) const;

    private:
        NetworkUtils *m_networkUtils;
        QString m_currentUuid;
        QString m_savePath;
    };

} // namespace EasyKiConverter

#endif // EXPORTER3DMODEL_H
