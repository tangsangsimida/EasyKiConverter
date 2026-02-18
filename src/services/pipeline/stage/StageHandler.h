#ifndef EASYKICONVERTER_STAGEHANDLER_H
#define EASYKICONVERTER_STAGEHANDLER_H

#include <QAtomicInt>
#include <QMutex>
#include <QObject>

namespace EasyKiConverter {

/**
 * @brief 流水线阶段处理器基类
 */
class StageHandler : public QObject {
    Q_OBJECT
public:
    explicit StageHandler(QAtomicInt& isCancelled, QObject* parent = nullptr)
        : QObject(parent), m_isCancelled(isCancelled) {}

    virtual ~StageHandler() = default;

    /**
     * @brief 启动阶段工作
     */
    virtual void start() = 0;

    /**
     * @brief 停止/取消阶段工作
     */
    virtual void stop() {
        // 基类默认实现为空，具体阶段可重写以实现立即中止
    }

protected:
    QAtomicInt& m_isCancelled;
};

}  // namespace EasyKiConverter

#endif  // EASYKICONVERTER_STAGEHANDLER_H
