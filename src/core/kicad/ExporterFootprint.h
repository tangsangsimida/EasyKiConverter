#ifndef EXPORTERFOOTPRINT_H
#define EXPORTERFOOTPRINT_H

#include "FootprintGraphicsGenerator.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTextStream>

namespace EasyKiConverter {

/**
 * @brief KiCad 封装导出器类
 *
 * 用于EasyEDA 封装数据导出KiCad 封装格式
 */
class ExporterFootprint : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
         *
     * @param parent 父对象
         */
    explicit ExporterFootprint(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ExporterFootprint() override;

    /**
     * @brief 导出封装库KiCad 格式（单D模型
     *
     * @param footprintData 封装数据
     * @param filePath 输出文件路径
     * @param model3DPath 3D模型路径
     * @return bool 是否成功
     */
    bool exportFootprint(const FootprintData& footprintData,
                         const QString& filePath,
                         const QString& model3DPath = QString());

    /**
     * @brief 导出封装库KiCad 格式（两D模型
         *
     * @param footprintData 封装数据
     * @param filePath 输出文件路径
     * @param model3DWrlPath WRL模型路径
     * @param model3DStepPath STEP模型路径
     * @return bool 是否成功
     */
    bool exportFootprint(const FootprintData& footprintData,
                         const QString& filePath,
                         const QString& model3DWrlPath,
                         const QString& model3DStepPath);

    /**
     * @brief 导出多个封装KiCad 封装
         *
     * @param footprints 封装列表
     * @param libName 库名称
         * @param filePath 输出文件路径
     * @return bool 是否成功
     */
    bool exportFootprintLibrary(const QList<FootprintData>& footprints,
                                const QString& libName,
                                const QString& filePath);

private:
    /**
     * @brief 生成 KiCad 封装
         *
     * @param libName 库名称
         * @return QString 头部文本
     */
    QString generateHeader(const QString& libName) const;

    /**
     * @brief 生成 KiCad 封装内容
     *
     * @param footprintData 封装数据
     * @param model3DPath 3D模型路径
     * @return QString 封装内容
     */
    QString generateFootprintContent(const FootprintData& footprintData, const QString& model3DPath = QString()) const;

    /**
     * @brief 生成 KiCad 封装内容（两D模型
         *
     * @param footprintData 封装数据
     * @param model3DWrlPath WRL模型路径
     * @param model3DStepPath STEP模型路径
     * @return QString 封装内容
     */
    QString generateFootprintContent(const FootprintData& footprintData,
                                     const QString& model3DWrlPath,
                                     const QString& model3DStepPath) const;

private:
    mutable FootprintGraphicsGenerator m_graphicsGenerator;
};

}  // namespace EasyKiConverter

#endif  // EXPORTERFOOTPRINT_H
