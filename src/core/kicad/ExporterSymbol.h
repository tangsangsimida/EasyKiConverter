#ifndef EXPORTERSYMBOL_H
#define EXPORTERSYMBOL_H

#include "SymbolGraphicsGenerator.h"
#include "models/SymbolData.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTextStream>

namespace EasyKiConverter {

/**
 * @brief KiCad 符号导出器类
 *
 * 用于EasyEDA 符号数据导出KiCad 符号库格
     */
class ExporterSymbol : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
         *
     * @param parent 父对象
         */
    explicit ExporterSymbol(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ExporterSymbol() override;

    /**
     * @brief 导出符号库KiCad 格式
     *
     * @param symbolData 符号数据
     * @param filePath 输出文件路径
     * @return bool 是否成功
     */
    bool exportSymbol(const SymbolData& symbolData, const QString& filePath);

    /**
     * @brief 导出多个符号KiCad 符号
         *
     * @param symbols 符号列表
     * @param libName 库名称
         * @param filePath 输出文件路径
     * @param appendMode 是否使用追加模式（默认true）
         * @param updateMode
     * 是否使用更新模式（默false）。如果为 true，则替换已存在的符号
     * @return bool 是否成功
     */
    bool exportSymbolLibrary(const QList<SymbolData>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode = true,
                             bool updateMode = false);

private:
    /**
     * @brief 生成 KiCad 符号
         *
     * @param libName 库名称
         * @return QString 头部文本
     */
    QString generateHeader(const QString& libName) const;

    /**
     * @brief 生成 KiCad 符号内容
     *
     * @param symbolData 符号数据
     * @param libName 库名称（用于 Footprint 前缀
         * @return QString 符号内容
     */
    QString generateSymbolContent(const SymbolData& symbolData, const QString& libName) const;

    /**
     * @brief 生成 KiCad 子符号（用于多部分符号）
     *
     * @param symbolData 符号数据
     * @param part 部分数据
     * @param symbolName 符号名称
     * @param libName 库名称
         * @param centerX 符号中心X坐标
     * @param centerY 符号中心Y坐标
     * @return QString 子符号文
         */
    QString generateSubSymbol(const SymbolData& symbolData,
                              const SymbolPart& part,
                              const QString& symbolName,
                              const QString& libName,
                              double centerX,
                              double centerY) const;

    /**
     * @brief 生成 KiCad 子符号（用于单部分符号）
     *
     * @param symbolData 符号数据
     * @param symbolName 符号名称
     * @param libName 库名称
         * @param centerX 符号中心X坐标
     * @param centerY 符号中心Y坐标
     * @return QString 子符号文
         */
    QString generateSubSymbol(const SymbolData& symbolData,
                              const QString& symbolName,
                              const QString& libName,
                              double centerX,
                              double centerY) const;

private:
    mutable SymbolGraphicsGenerator m_graphicsGenerator;  // 图形元素生成器
};

}  // namespace EasyKiConverter

#endif  // EXPORTERSYMBOL_H
