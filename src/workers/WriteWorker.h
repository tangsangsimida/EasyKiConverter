#ifndef WRITEWORKER_H
#define WRITEWORKER_H

#include <QObject>
#include <QRunnable>
#include "models/ComponentExportStatus.h"
#include "core/kicad/ExporterSymbol.h"
#include "core/kicad/ExporterFootprint.h"
#include "core/kicad/Exporter3DModel.h"

namespace EasyKiConverter
{

    /**
     * @brief 写入工作线程
     *
     * 负责将数据写入文件（磁盘I/O密集型任务）
     */
    class WriteWorker : public QObject, public QRunnable
    {
        Q_OBJECT

    public:
        /**
         * @brief 构造函�?
         * @param status 导出状态（使用 QSharedPointer 避免拷贝�?
         * @param outputPath 输出路径
         * @param libName 库名�?
         * @param exportSymbol 是否导出符号
         * @param exportFootprint 是否导出封装
         * @param exportModel3D 是否导出3D模型
         * @param debugMode 是否启用调试模式
         * @param parent 父对�?
         */
        explicit WriteWorker(
            QSharedPointer<ComponentExportStatus> status,
            const QString &outputPath,
            const QString &libName,
            bool exportSymbol,
            bool exportFootprint,
            bool exportModel3D,
            bool debugMode,
            QObject *parent = nullptr);

        /**
         * @brief 析构函数
         */
        ~WriteWorker() override;

        /**
         * @brief 执行写入任务
         */
        void run() override;

    signals:
        /**
         * @brief 写入完成信号
         * @param status 导出状态（使用 QSharedPointer 避免拷贝�?
         */
        void writeCompleted(QSharedPointer<ComponentExportStatus> status);

    private:
        /**
         * @brief 写入符号文件
         * @param status 导出状�?
         * @return bool 是否成功
         */
        bool writeSymbolFile(ComponentExportStatus &status);

        /**
         * @brief 写入封装文件
         * @param status 导出状�?
         * @return bool 是否成功
         */
        bool writeFootprintFile(ComponentExportStatus &status);

        /**
         * @brief 写入3D模型文件
         * @param status 导出状�?
         * @return bool 是否成功
         */
        bool write3DModelFile(ComponentExportStatus &status);

        /**
         * @brief 创建输出目录
         * @param path 路径
         * @return bool 是否成功
         */
        bool createOutputDirectory(const QString &path);

        /**
         * @brief 导出调试数据
         * @param status 导出状�?
         * @return bool 是否成功
         */
        bool exportDebugData(ComponentExportStatus &status);

    private:
        QSharedPointer<ComponentExportStatus> m_status;
        QString m_outputPath;
        QString m_libName;
        bool m_exportSymbol;
        bool m_exportFootprint;
        bool m_exportModel3D;
        bool m_debugMode;

        ExporterSymbol m_symbolExporter;
        ExporterFootprint m_footprintExporter;
        Exporter3DModel m_model3DExporter;
    };

} // namespace EasyKiConverter

#endif // WRITEWORKER_H
