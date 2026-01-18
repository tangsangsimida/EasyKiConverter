#ifndef IEXPORTERSYMBOL_H
#define IEXPORTERSYMBOL_H

#include <QObject>
#include <QString>
#include <QList>
#include "src/models/SymbolData.h"

namespace EasyKiConverter {

/**
 * @brief 符号导出器接口类
 *
 * 定义了符号导出的标准接口
 * 用于依赖注入和单元测试
 */
class IExporterSymbol : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     *
     * @param parent 父对象
     */
    explicit IExporterSymbol(QObject *parent = nullptr) : QObject(parent) {}

    /**
     * @brief 析构函数
     */
    virtual ~IExporterSymbol() = default;

    /**
     * @brief 导出符号为 KiCad 格式
     *
     * @param symbolData 符号数据
     * @param filePath 输出文件路径
     * @return bool 是否成功
     */
    virtual bool exportSymbol(const SymbolData &symbolData, const QString &filePath) = 0;

    /**
     * @brief 导出多个符号为 KiCad 符号库（追加模式）
     *
     * @param symbols 符号列表
     * @param libName 库名称
     * @param filePath 输出文件路径
     * @param appendMode 是否使用追加模式（默认 true）
     * @return bool 是否成功
     */
    virtual bool exportSymbolLibrary(const QList<SymbolData> &symbols, const QString &libName, const QString &filePath, bool appendMode = true) = 0;
};

} // namespace EasyKiConverter

#endif // IEXPORTERSYMBOL_H