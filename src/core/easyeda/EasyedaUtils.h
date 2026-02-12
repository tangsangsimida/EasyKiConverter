#ifndef EASYEDAUTILS_H
#define EASYEDAUTILS_H

#include <QString>
#include <QStringList>

namespace EasyKiConverter {

class EasyedaUtils {
public:
    static QStringList parseDataString(const QString& data) {
        return data.split("~", Qt::KeepEmptyParts);
    }

    static bool stringToBool(const QString& str) {
        if (str.isEmpty() || str == "0" || str.toLower() == "false" || str.toLower() == "none" ||
            str.toLower() == "transparent") {
            return false;
        }
        return true;
    }
};

}  // namespace EasyKiConverter

#endif  // EASYEDAUTILS_H
