#ifndef KICADEXPORTMETADATA_H
#define KICADEXPORTMETADATA_H

#include <QString>

namespace EasyKiConverter::KiCadExportMetadata {

inline QString generatorName() {
    return QStringLiteral("EasyKiConverter");
}

inline QString generatorVersion() {
    return QStringLiteral("3.1.8");
}

inline QString goldenMetadataComment() {
    return QStringLiteral("; (generator_version \"%1\")\n").arg(generatorVersion());
}

}  // namespace EasyKiConverter::KiCadExportMetadata

#endif  // KICADEXPORTMETADATA_H
