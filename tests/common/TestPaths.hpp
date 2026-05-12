#pragma once

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

namespace EasyKiConverter::Test {

class TestPaths {
public:
    static QString testsRoot() {
#ifdef EASYKICONVERTER_TEST_SOURCE_DIR
        return QDir::cleanPath(QString::fromUtf8(EASYKICONVERTER_TEST_SOURCE_DIR));
#else
        return QDir::cleanPath(QDir::currentPath() + QStringLiteral("/tests"));
#endif
    }

    static QString fixturePath(const QString& relativePath) {
        return underTestsRoot(QStringLiteral("fixtures"), relativePath);
    }

    static QString goldenPath(const QString& relativePath) {
        return underTestsRoot(QStringLiteral("golden"), relativePath);
    }

    static QByteArray readBytes(const QString& path, QString* errorMessage = nullptr) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            setError(errorMessage, QStringLiteral("Unable to open file '%1': %2").arg(path, file.errorString()));
            return {};
        }
        return file.readAll();
    }

    static QString readText(const QString& path, QString* errorMessage = nullptr) {
        return QString::fromUtf8(readBytes(path, errorMessage));
    }

    static QJsonObject readJsonObject(const QString& path, QString* errorMessage = nullptr) {
        QString readError;
        const QByteArray bytes = readBytes(path, &readError);
        if (!readError.isEmpty()) {
            setError(errorMessage, readError);
            return {};
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            setError(errorMessage,
                     QStringLiteral("Unable to parse JSON file '%1': %2").arg(path, parseError.errorString()));
            return {};
        }

        if (!document.isObject()) {
            setError(errorMessage, QStringLiteral("JSON file '%1' root must be an object").arg(path));
            return {};
        }

        return document.object();
    }

    static bool compareTextToGolden(const QString& actual,
                                    const QString& relativeGoldenPath,
                                    QString* errorMessage = nullptr) {
        QString readError;
        const QString expectedPath = goldenPath(relativeGoldenPath);
        QString expected = readText(expectedPath, &readError);
        if (!readError.isEmpty()) {
            setError(errorMessage, readError);
            return false;
        }

        const QString normalizedExpected = normalizeLineEndings(expected);
        const QString normalizedActual = normalizeLineEndings(actual);
        if (normalizedExpected == normalizedActual) {
            return true;
        }

        setError(errorMessage, mismatchMessage(normalizedExpected, normalizedActual, expectedPath));
        return false;
    }

    static QString normalizeLineEndings(QString value) {
        value.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        value.replace(QChar('\r'), QChar('\n'));
        return value;
    }

private:
    static QString underTestsRoot(const QString& directory, const QString& relativePath) {
        QDir root(testsRoot());
        return QDir::cleanPath(root.filePath(directory + QChar('/') + relativePath));
    }

    static void setError(QString* errorMessage, const QString& value) {
        if (errorMessage != nullptr) {
            *errorMessage = value;
        }
    }

    static QString mismatchMessage(const QString& expected, const QString& actual, const QString& expectedPath) {
        const int maxLength = qMin(expected.size(), actual.size());
        int firstMismatch = 0;
        while (firstMismatch < maxLength && expected.at(firstMismatch) == actual.at(firstMismatch)) {
            ++firstMismatch;
        }

        if (expected.size() != actual.size() && firstMismatch == maxLength) {
            firstMismatch = maxLength;
        }

        const int contextStart = qMax(0, firstMismatch - 40);
        const int contextLength = 100;
        const QString expectedContext = visibleSnippet(expected.mid(contextStart, contextLength));
        const QString actualContext = visibleSnippet(actual.mid(contextStart, contextLength));

        return QStringLiteral(
                   "Golden file mismatch for '%1' at offset %2 (expected length %3, actual length %4)\n"
                   "Expected near mismatch: \"%5\"\n"
                   "Actual near mismatch:   \"%6\"")
            .arg(expectedPath)
            .arg(firstMismatch)
            .arg(expected.size())
            .arg(actual.size())
            .arg(expectedContext, actualContext);
    }

    static QString visibleSnippet(QString value) {
        value.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
        value.replace(QStringLiteral("\t"), QStringLiteral("\\t"));
        return value;
    }
};

}  // namespace EasyKiConverter::Test
