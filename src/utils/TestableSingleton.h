#ifndef TESTABLESINGLETON_H
#define TESTABLESINGLETON_H

#include <QMutex>
#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief Template for testable singleton pattern
 *
 * Usage:
 *   class MyService : public TestableSingleton<MyService> {
 *       friend class TestableSingleton<MyService>;
 *       MyService() = default;
 *   public:
 *       void doWork();
 *   };
 *
 * Testing:
 *   MyService::setTestingInstance(new MockMyService());
 *   // run tests
 *   MyService::destroyInstance();
 */
template <typename T>
class TestableSingleton {
public:
    static T& instance() {
        if (s_mockInstance) {
            return *s_mockInstance;
        }

        QMutexLocker locker(&s_mutex);
        if (!s_instance) {
            s_instance = new T();
        }
        return *s_instance;
    }

    static void destroyInstance() {
        QMutexLocker locker(&s_mutex);
        delete s_instance;
        s_instance = nullptr;
    }

    static void setTestingInstance(T* mock) {
        QMutexLocker locker(&s_mutex);
        delete s_instance;
        s_instance = mock;
        s_mockInstance = mock;
    }

    static void clearMockInstance() {
        QMutexLocker locker(&s_mutex);
        s_mockInstance = nullptr;
    }

    static bool hasInstance() {
        QMutexLocker locker(&s_mutex);
        return s_instance != nullptr;
    }

    static bool hasMockInstance() {
        return s_mockInstance != nullptr;
    }

protected:
    TestableSingleton() = default;
    virtual ~TestableSingleton() = default;

private:
    static QMutex s_mutex;
    static T* s_instance;
    static T* s_mockInstance;
};

template <typename T>
QMutex TestableSingleton<T>::s_mutex;

template <typename T>
T* TestableSingleton<T>::s_instance = nullptr;

template <typename T>
T* TestableSingleton<T>::s_mockInstance = nullptr;

}  // namespace EasyKiConverter

#endif  // TESTABLESINGLETON_H
