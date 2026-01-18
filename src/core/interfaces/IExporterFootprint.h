#ifndef IEXPORTERFOOTPRINT_H
#define IEXPORTERFOOTPRINT_H

#include <QObject>
#include <QString>
#include <QList>
#include "src/models/FootprintData.h"

namespace EasyKiConverter {

/**
 * @brief 封装导出器接口类
 *
 * 定义了封装导出的标准接口
 * 用于依赖注入和单元测试
 */
class IExporterFootprint : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     *
     * @param parent 父对象
     */
    explicit IExporterFootprint(QObject *parent = nullptr) : QObject(parent) {}

    /**
     * @brief 析构函数
     */
    virtual ~IExporterFootprint() = default;

    /**
     * @brief 导出封装为 KiCad 格式
     *
     * @param footprintData 封装数据
     * @param filePath 输出文件路径
     * @param model3DPath 3D 模型路径（可选）
     * @return bool 是否成功
     */
    virtual bool exportFootprint(const FootprintData &footprintData, const QString &filePath, const QString &model3DPath = QString()) = 0;

    /**
     * @brief 导出多个封装为 KiCad 封装库
     *
     * @param footprints 封装列表
     * @param libName 库名称
     * @param filePath 输出文件路径
     * @return bool 是否成功
     */
    virtual bool exportFootprintLibrary(const QList<FootprintData> &footprints, const QString &libName, const QString &filePath) = 0;
};

} // namespace EasyKiConverter

#endif // IEXPORTERFOOTPRINT_H