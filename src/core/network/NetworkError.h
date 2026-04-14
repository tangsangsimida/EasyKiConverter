#ifndef NETWORKERROR_H
#define NETWORKERROR_H

#include <QDateTime>
#include <QMap>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief Unified network error representation
 */
class NetworkError {
public:
    enum class Severity { Info, Warning, Error, Critical };

    enum class Category { Network, Parsing, Validation, System };

    NetworkError() = default;

    NetworkError(const QString& componentId,
                 const QString& message,
                 Severity severity,
                 Category category,
                 int statusCode = 0)
        : componentId(componentId)
        , message(message)
        , severity(severity)
        , category(category)
        , statusCode(statusCode)
        , timestamp(QDateTime::currentDateTime()) {}

    QString componentId;
    QString message;
    Severity severity = Severity::Warning;
    Category category = Category::Network;
    int statusCode = 0;
    QDateTime timestamp;
    QMap<QString, QString> details;

    QString toString() const {
        return QString("[%1] %2 (%3): %4 (status=%5)")
            .arg(static_cast<int>(severity))
            .arg(categoryToString(category))
            .arg(componentId)
            .arg(message)
            .arg(statusCode);
    }

    static QString severityToString(Severity s) {
        switch (s) {
            case Severity::Info:
                return "Info";
            case Severity::Warning:
                return "Warning";
            case Severity::Error:
                return "Error";
            case Severity::Critical:
                return "Critical";
        }
        return "Unknown";
    }

    static QString categoryToString(Category c) {
        switch (c) {
            case Category::Network:
                return "Network";
            case Category::Parsing:
                return "Parsing";
            case Category::Validation:
                return "Validation";
            case Category::System:
                return "System";
        }
        return "Unknown";
    }
};

}  // namespace EasyKiConverter

#endif  // NETWORKERROR_H
