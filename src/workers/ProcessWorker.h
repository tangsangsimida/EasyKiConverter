#ifndef PROCESSWORKER_H
#define PROCESSWORKER_H

#include <QObject>
#include <QRunnable>
#include "src/models/ComponentExportStatus.h"

namespace EasyKiConverter {

/**
 * @brief 处理工作线程
 *
 * 负责解析原始数据并转换为KiCad格式（CPU密集型任务）
 */
class ProcessWorker : public QObject, public QRunnable
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param status 从FetchWorker接收的状态
     * @param parent 父对象
     */
    explicit ProcessWorker(const ComponentExportStatus &status, QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ProcessWorker() override;

    /**
     * @brief 执行处理任务
     */
    void run() override;

signals:
    /**
     * @brief 处理完成信号
     * @param status 导出状态
     */
    void processCompleted(const ComponentExportStatus &status);

private:
    /**
     * @brief 解析组件信息
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool parseComponentInfo(ComponentExportStatus &status);

    /**
     * @brief 解析CAD数据
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool parseCadData(ComponentExportStatus &status);

    /**
     * @brief 解析3D模型数据
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool parse3DModelData(ComponentExportStatus &status);

private:
    ComponentExportStatus m_status;
};

} // namespace EasyKiConverter

#endif // PROCESSWORKER_H