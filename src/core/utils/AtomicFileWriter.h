#ifndef ATOMICFILEWRITER_H
#define ATOMICFILEWRITER_H

#include <QDir>
#include <QFile>
#include <QMutex>
#include <QString>

#include <functional>

namespace EasyKiConverter {

class AtomicFileWriter {
public:
    using WriteFunc = std::function<bool(const QString& tempPath)>;

    AtomicFileWriter() = default;
    ~AtomicFileWriter() = default;

    static bool writeAtomically(const QString& tempDir,
                                const QString& finalPath,
                                const QString& suffix,
                                WriteFunc writeFunc);

    static bool writeAtomically(const QString& tempDir,
                                const QString& finalPath,
                                const QString& suffix,
                                const QByteArray& data);

    static bool createDirectory(const QString& path);

private:
    static QString generateTempPath(const QString& tempDir, const QString& prefix, const QString& suffix);

    static QMutex s_fileWriteMutex;
};

}  // namespace EasyKiConverter

#endif  // ATOMICFILEWRITER_H