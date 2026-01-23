#ifndef EXPORTWORKER_H
#define EXPORTWORKER_H

#include <QObject>
#include <QRunnable>
#include <QSharedPointer>
#include "models/ComponentData.h"
#include "models/SymbolData.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"

namespace EasyKiConverter
{

    /**
     * @brief 导出工作线程
     *
     * 用于在后台线程中执行元件导出任务
     */
    class ExportWorker : public QObject, public QRunnable
    {
        Q_OBJECT

    public:
        /**
         * @brief 构造函�?
         * @param componentData 元件数据
         * @param symbolData 符号数据
         * @param footprintData 封装数据
         * @param outputPath 输出路径
         * @param libName 库名�?
         * @param exportSymbol 是否导出符号
         * @param exportFootprint 是否导出封装
         * @param exportModel3D 是否导出3D模型
         * @param parent 父对�?
         */
        explicit ExportWorker(
            const QString &componentId,
            QSharedPointer<SymbolData> symbolData,
            QSharedPointer<FootprintData> footprintData,
            const QString &outputPath,
            const QString &libName,
            bool exportSymbol,
            bool exportFootprint,
            bool exportModel3D,
            QObject *parent = nullptr);

        /**
         * @brief 析构函数
         */
        ~ExportWorker() override;

        /**
         * @brief 执行导出任务
         */
        void run() override;

    signals:
        /**
         * @brief 导出完成信号
         * @param componentId 元件ID
         * @param success 是否成功
         * @param message 消息
         */
        void exportFinished(const QString &componentId, bool success, const QString &message);

        /**
         * @brief 导出进度信号
         * @param componentId 元件ID
         * @param progress 进度�?-100�?
         */
        void exportProgress(const QString &componentId, int progress);

    private:
        /**
         * @brief 导出符号�?
         * @return bool 是否成功
         */
        bool exportSymbolLibrary();

        /**
         * @brief 导出封装�?
         * @param model3DPath 3D模型路径
         * @return bool 是否成功
         */
        bool exportFootprintLibrary(const QString &model3DPath);

        /**
         * @brief 导出3D模型
         * @param model3DData 3D模型数据
         * @param outputPath 输出路径
         * @return bool 是否成功
         */
        bool export3DModel(const Model3DData &model3DData, const QString &outputPath);

        /**
         * @brief 创建输出目录
         * @return bool 是否成功
         */
        bool createOutputDirectories();

    private:
        QString m_componentId;
        QSharedPointer<SymbolData> m_symbolData;
        QSharedPointer<FootprintData> m_footprintData;
        QString m_outputPath;
        QString m_libName;
        bool m_exportSymbol;
        bool m_exportFootprint;
        bool m_exportModel3D;
    };

} // namespace EasyKiConverter

#endif // EXPORTWORKER_H
