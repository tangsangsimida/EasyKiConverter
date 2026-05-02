#include "KiCadTableUtils.h"

namespace EasyKiConverter {
namespace KiCadTableUtils {

QString escapeTableValue(const QString& value) {
    QString escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    return escaped;
}

}  // namespace KiCadTableUtils
}  // namespace EasyKiConverter
