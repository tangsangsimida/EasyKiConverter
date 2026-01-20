#ifndef COMPONENTEXPORTSTATUS_H
#define COMPONENTEXPORTSTATUS_H

#include <QString>
#include <QSharedPointer>
#include "src/models/ComponentData.h"
#include "src/models/SymbolData.h"
#include "src/models/FootprintData.h"
#include "src/models/Model3DData.h"

namespace EasyKiConverter {

/**
 * @brief 元件导出状态
 *
 * 跟踪元件在流水线各阶段的状态
 */
struct ComponentExportStatus
{
    QString componentId;  // 元件ID
    
    // 原始数据（抓取阶段）
    QByteArray componentInfoRaw;  // 组件信息原始数据
    QByteArray cinfoJsonRaw;      // cinfo JSON 原始数据
    QByteArray cadDataRaw;        // CAD数据原始数据
    QByteArray cadJsonRaw;        // cad JSON 原始数据
    QByteArray advJsonRaw;        // adv JSON 原始数据
    QByteArray model3DObjRaw;     // 3D模型OBJ原始数据
    QByteArray model3DStepRaw;    // 3D模型STEP原始数据
    
    // 解析后的数据（处理阶段）
    QSharedPointer<ComponentData> componentData;
    QSharedPointer<SymbolData> symbolData;
    QSharedPointer<FootprintData> footprintData;
    QSharedPointer<Model3DData> model3DData;
    
    // 抓取阶段状态
    bool fetchSuccess = false;
    QString fetchMessage;
    
    // 处理阶段状态
    bool processSuccess = false;
    QString processMessage;
    
    // 写入阶段状态
    bool writeSuccess = false;
    QString writeMessage;

    // 调试日志（仅在调试模式下使用）
    QStringList debugLog;

    // 是否需要导出3D模型
    bool need3DModel = false;
    
    /**
     * @brief 检查是否完全成功
     * @return bool
     */
    bool isCompleteSuccess() const
    {
        return fetchSuccess && processSuccess && writeSuccess;
    }
    
    /**
     * @brief 获取失败阶段
     * @return QString 失败阶段名称
     */
    QString getFailedStage() const
    {
        if (!fetchSuccess) return "Fetch";
        if (!processSuccess) return "Process";
        if (!writeSuccess) return "Write";
        return "";
    }

    /**
     * @brief 添加调试日志
     * @param message 日志消息
     */
    void addDebugLog(const QString &message)
    {
        debugLog.append(message);
    }
};

} // namespace EasyKiConverter

#endif // COMPONENTEXPORTSTATUS_H