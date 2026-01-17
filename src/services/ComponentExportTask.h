#ifndef COMPONENTEXPORTTASK_H
#define COMPONENTEXPORTTASK_H

#include <QObject>
#include <QRunnable>
#include <QString>
#include "src/models/ComponentData.h"
#include "services/ExportService.h"

namespace EasyKiConverter
{

    /**
     * @brief 元件导出任务类
     *
     * 使用信号/槽机制传递结果
     */
    class ComponentExportTask : public QObject, public QRunnable
    {
        Q_OBJECT

    public:
        /**
         * @brief 构造函数
         *
         * @param componentId 元件ID
         * @param componentData 元件数据
         * @param options 导出选项
         * @param parent 父对象
         */
        explicit ComponentExportTask(
            const QString &componentId,
            const ComponentData &componentData,
            const ExportOptions &options,
            QObject *parent = nullptr);

        /**
         * @brief 析构函数
         */
        ~ComponentExportTask() override;

        /**
         * @brief 执行任务
         */
        void run() override;

    signals:
        /**
         * @brief 导出完成信号
         *
         * @param componentId 元件ID
         * @param success 是否成功
         * @param message 消息
         */
        void exportFinished(const QString &componentId, bool success, const QString &message);

    private:
        /**
         * @brief 执行导出操作
         *
         * @return bool 是否成功
         */
        bool executeExport();

        /**
         * @brief 生成符号文件路径
         *
         * @return QString 文件路径
         */
        QString getSymbolFilePath() const;

        /**
         * @brief 生成封装文件路径
         *
         * @return QString 文件路径
         */
        QString getFootprintFilePath() const;

        /**
         * @brief 生成3D模型文件路径
         *
         * @return QString 文件路径
         */
        QString getModel3DFilePath() const;

        /**
         * @brief 创建输出目录
         *
         * @return bool 是否成功
         */
        bool createOutputDirectory() const;

    private:
        QString m_componentId;
        ComponentData m_componentData;
        ExportOptions m_options;
    };

} // namespace EasyKiConverter

#endif // COMPONENTEXPORTTASK_H
