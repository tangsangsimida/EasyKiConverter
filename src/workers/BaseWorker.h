#ifndef BASEWORKER_H
#define BASEWORKER_H

#include <QObject>
#include <QRunnable>

namespace EasyKiConverter {

/**
 * @brief Base class for all workers with unified lifecycle management
 *
 * All workers should inherit from this class and use setAutoDelete(false)
 * to allow explicit lifecycle management via deleteLater() signals.
 *
 * Usage:
 *   class MyWorker : public BaseWorker {
 *       MyWorker() { setAutoDelete(false); }
 *       void run() override { ... emit completed(); }
 *   signals:
 *       void completed();
 *   };
 *
 * Caller should connect:
 *   connect(worker, &MyWorker::completed, worker, &QObject::deleteLater, Qt::QueuedConnection);
 *   QThreadPool::globalInstance()->start(worker);
 */
class BaseWorker : public QObject, public QRunnable {
protected:
    BaseWorker() {
        setAutoDelete(false);  // Let signals manage lifecycle
    }

public:
    ~BaseWorker() override = default;
};

}  // namespace EasyKiConverter

#endif  // BASEWORKER_H
