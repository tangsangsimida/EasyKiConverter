#ifndef COMPONENTLISTDATACOORDINATOR_H
#define COMPONENTLISTDATACOORDINATOR_H

#include "models/ComponentData.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

class ComponentListViewModel;

/**
 * @brief 协调组件服务结果到列表项状态的转换。
 *
 * 该协作者只处理异步基础信息、CAD、LCSC、数据手册和错误回调，
 * 列表模型仍负责信号连接、索引查找和对外暴露的槽函数接口。
 */
class ComponentListDataCoordinator final {
public:
    /** @brief 合并组件基础信息到列表项。 */
    static void handleComponentInfo(ComponentListViewModel& viewModel,
                                    const QString& componentId,
                                    const ComponentData& data);

    /** @brief 合并 CAD 数据并推进验证状态。 */
    static void handleCadData(ComponentListViewModel& viewModel, const QString& componentId, const ComponentData& data);

    /** @brief 根据请求错误更新验证或预览状态。 */
    static void handleFetchError(ComponentListViewModel& viewModel, const QString& componentId, const QString& error);

    /** @brief 合并 LCSC 返回的制造商、数据手册和图片信息。 */
    static void handleLcscData(ComponentListViewModel& viewModel,
                               const QString& componentId,
                               const QString& manufacturerPart,
                               const QString& datasheetUrl,
                               const QStringList& imageUrls);

    /** @brief 保存数据手册内容并推断数据格式。 */
    static void handleDatasheet(ComponentListViewModel& viewModel,
                                const QString& componentId,
                                const QByteArray& datasheetData);
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTDATACOORDINATOR_H
