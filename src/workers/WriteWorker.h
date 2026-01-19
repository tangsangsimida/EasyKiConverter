#ifndef WRITESHWORKER_H
#define WRITESHWORKER_H

#include <QObject>
#include <QRunnable>
#include <QFile>
#include <QDir>
#include "src/models/ComponentExportStatus.h"
#include "src/core/kicad/ExporterSymbol.h"
#include "src/core/kicad/ExporterFootprint.h"
#include "src/core/kicad/Exporter3DModel.h"

namespace EasyKiConverter {

/**
 * @brief 写入工作线程
 *
 * 负责将处理后的数据写入文件（磁盘I/O密集型任务）
 */
class WriteWorker : public QObject, public QRunnable
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param status 从ProcessWorker接收的状态
     * @param outputPath 输出路径
     * @param libName 库名称
     * @param exportSymbol 是否导出符号
     * @param exportFootprint 是否导出封装
     * @param exportModel3D 是否导出3D模型
     * @param parent 父对象
     */
    explicit WriteWorker(
        const ComponentExportStatus &status,
        const QString &outputPath,
        const QString &libName,
        bool exportSymbol,
        bool exportFootprint,
        bool exportModel3D,
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
     * @param status 导出状态
     */
    void writeCompleted(const ComponentExportStatus &status);

private:
    /**
     * @brief 写入符号文件
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool writeSymbolFile(ComponentExportStatus &status);

    /**
     * @brief 写入封装文件
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool writeFootprintFile(ComponentExportStatus &status);

    /**
     * @brief 写入3D模型文件
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool write3DModelFile(ComponentExportStatus &status);

    /**
     * @brief 创建输出目录
     * @param path 路径
     * @return bool 是否成功
     */
    bool createOutputDirectory(const QString &path);

private:
    ComponentExportStatus m_status;
    QString m_outputPath;
    QString m_libName;
    bool m_exportSymbol;
    bool m_exportFootprint;
    bool m_exportModel3D;

    // 导出器
    ExporterSymbol m_symbolExporter;
    ExporterFootprint m_footprintExporter;
    Exporter3DModel m_modelExporter;
};

} // namespace EasyKiConverter

#endif // WRITESHWORKER_H