#include "MemoryMonitor.h"

#include <QByteArray>
#include <QDebug>
#include <QFile>

#if defined(Q_OS_LINUX)
#    include <unistd.h>
#elif defined(Q_OS_WIN) || defined(_WIN32)
// clang-format off
// Order matters: windows.h must be included before other Windows API headers
#    if defined(__MINGW32__) || defined(__MINGW64__)
#        include <qt_windows.h>

#        include <psapi.h>
#        pragma comment(lib, "psapi")
#    else
#        include <windows.h>
#        include <psapi.h>
#        pragma comment(lib, "psapi")
#    endif
// clang-format on
#elif defined(Q_OS_MAC)
#    include <mach/mach.h>
#    include <mach/task_info.h>
#endif

namespace EasyKiConverter {

qint64 MemoryMonitor::s_peakUsage = 0;

qint64 MemoryMonitor::getCurrentUsage() {
#if defined(Q_OS_LINUX)
    QFile file("/proc/self/status");
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open /proc/self/status:" << file.errorString();
        return 0;
    }
    QByteArray content = file.readAll();
    file.close();

    QList<QByteArray> lines = content.split('\n');
    for (const QByteArray& line : lines) {
        if (line.startsWith("VmRSS:")) {
            QByteArray value = line.mid(6).trimmed();
            if (value.endsWith("kB") || value.endsWith("kb")) {
                value = value.left(value.length() - 2);
            } else if (value.endsWith("k")) {
                value = value.left(value.length() - 1);
            }
            value = value.trimmed();
            bool ok = false;
            qint64 rssKb = value.toLongLong(&ok);
            if (ok) {
                return rssKb * 1024;
            }
        }
    }
    return 0;

#elif defined(Q_OS_WIN) || defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<qint64>(pmc.WorkingSetSize);
    }
    return 0;

#elif defined(Q_OS_MAC)
    struct task_basic_info info;
    mach_msg_type_number_t size = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &size) == KERN_SUCCESS) {
        return static_cast<qint64>(info.resident_size);
    }
    return 0;

#else
    return 0;
#endif
}

qint64 MemoryMonitor::getPeakUsage() {
    return s_peakUsage;
}

void MemoryMonitor::updatePeak() {
    qint64 currentMemory = getCurrentUsage();
    if (currentMemory > s_peakUsage) {
        s_peakUsage = currentMemory;
    }
}

void MemoryMonitor::resetPeak() {
    s_peakUsage = 0;
}

}  // namespace EasyKiConverter
