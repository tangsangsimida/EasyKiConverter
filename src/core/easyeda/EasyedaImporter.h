#ifndef EASYEDAIMPORTER_H
#define EASYEDAIMPORTER_H

#include "models/FootprintData.h"
#include "models/SymbolData.h"

#include <QJsonObject>
#include <QObject>
#include <QSharedPointer>

namespace EasyKiConverter {

/**
 * @brief EasyEDA 数据导入器类
 *
 * 用于解析EasyEDA 的 JSON 数据并转换为内部数据模型
 */
class EasyedaImporter : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
         *
     * @param parent 父对象
         */
    explicit EasyedaImporter(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~EasyedaImporter() override;

    /**
     * @brief 导入符号数据
     *
     * @param cadData EasyEDA 的 JSON 数据
     * @return QSharedPointer<SymbolData> 符号数据模型
     */
    QSharedPointer<SymbolData> importSymbolData(const QJsonObject& cadData);

    /**
     * @brief 导入封装数据
         *
     * @param cadData EasyEDA 的 JSON 数据
     * @return QSharedPointer<FootprintData> 封装数据模型
     */
    QSharedPointer<FootprintData> importFootprintData(const QJsonObject& cadData);
};

}  // namespace EasyKiConverter

#endif  // EASYEDAIMPORTER_H
