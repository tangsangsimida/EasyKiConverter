#ifndef BASECONVERTER_H
#define BASECONVERTER_H

#include <QObject>
#include <QString>

namespace EasyKiConverter {

class CliContext;
class CliPrinter;

/**
 * @brief 转换器基类
 *
 * 所有 CLI 转换器的基类，提供通用的转换逻辑。
 */
class BaseConverter : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param context CLI 上下文
     * @param parent 父对象指针
     */
    explicit BaseConverter(CliContext* context, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~BaseConverter() override = default;

    /**
     * @brief 执行转换
     * @return 成功返回 true，失败返回 false
     */
    virtual bool execute() = 0;

    /**
     * @brief 获取错误信息
     * @return 错误信息字符串
     */
    QString errorMessage() const {
        return m_errorMessage;
    }

signals:
    /**
     * @brief 进度更新
     * @param progress 进度百分比
     * @param message 进度消息
     */
    void progressUpdated(int progress, const QString& message);

    /**
     * @brief 转换完成
     * @param success 是否成功
     * @param message 完成消息
     */
    void conversionFinished(bool success, const QString& message);

protected:
    /**
     * @brief 获取 CLI 上下文
     * @return CLI 上下文指针
     */
    CliContext* context() const {
        return m_context;
    }

    /**
     * @brief 获取打印机
     * @return 打印机指针
     */
    const CliPrinter* printer() const {
        return m_printer;
    }

    /**
     * @brief 设置错误信息
     * @param message 错误信息
     */
    void setError(const QString& message) {
        m_errorMessage = message;
    }

    /**
     * @brief 输出消息
     * @param message 消息内容
     */
    void printMessage(const QString& message) const;

    /**
     * @brief 输出进度条
     * @param progress 进度百分比
     */
    void printProgressBar(int progress) const;

private:
    CliContext* m_context;
    CliPrinter* m_printer;
    QString m_errorMessage;
};

}  // namespace EasyKiConverter

#endif  // BASECONVERTER_H
