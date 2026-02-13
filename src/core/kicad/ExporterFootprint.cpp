#include "ExporterFootprint.h"

#include "core/utils/GeometryUtils.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace EasyKiConverter {

ExporterFootprint::ExporterFootprint(QObject* parent) : QObject(parent) {}

ExporterFootprint::~ExporterFootprint() {}

bool ExporterFootprint::exportFootprint(const FootprintData& footprintData,
                                        const QString& filePath,
                                        const QString& model3DPath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    QString content = generateFootprintContent(footprintData, model3DPath);

    out << content;
    file.flush();
    file.close();

    qDebug() << "Footprint exported to:" << filePath;
    return true;
}

bool ExporterFootprint::exportFootprint(const FootprintData& footprintData,
                                        const QString& filePath,
                                        const QString& model3DWrlPath,
                                        const QString& model3DStepPath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    QString content = generateFootprintContent(footprintData, model3DWrlPath, model3DStepPath);

    out << content;
    file.flush();
    file.close();

    qDebug() << "Footprint exported to:" << filePath << "with 2 3D models";
    return true;
}

bool ExporterFootprint::exportFootprintLibrary(const QList<FootprintData>& footprints,
                                               const QString& libName,
                                               const QString& filePath) {
    qDebug() << "=== Export Footprint Library ===";
    qDebug() << "Library name:" << libName;
    qDebug() << "Output path:" << filePath;
    qDebug() << "Footprint count:" << footprints.count();

    QDir libDir(filePath);

    if (!libDir.exists()) {
        qDebug() << "Creating footprint library directory:" << filePath;
        if (!libDir.mkpath(".")) {
            qWarning() << "Failed to create footprint library directory:" << filePath;
            return false;
        }
    }

    QSet<QString> existingFootprintNames;
    QStringList existingFiles = libDir.entryList(QStringList("*.kicad_mod"), QDir::Files);
    for (const QString& fileName : existingFiles) {
        QString footprintName = fileName;
        footprintName.remove(".kicad_mod");
        existingFootprintNames.insert(footprintName);
        qDebug() << "Found existing footprint:" << footprintName;
    }

    qDebug() << "Existing footprints count:" << existingFootprintNames.count();

    int exportedCount = 0;
    int overwrittenCount = 0;
    for (const FootprintData& footprint : footprints) {
        QString footprintName = footprint.info().name;
        QString fileName = footprintName + ".kicad_mod";
        QString fullPath = libDir.filePath(fileName);

        bool exists = existingFootprintNames.contains(footprintName);
        if (exists) {
            qDebug() << "Overwriting existing footprint:" << footprintName;
            overwrittenCount++;
        } else {
            qDebug() << "Exporting new footprint:" << footprintName;
            exportedCount++;
        }

        QString content = generateFootprintContent(footprint);

        QFile file(fullPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Failed to open file for writing:" << fullPath;
            continue;
        }

        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << content;
        file.close();

        qDebug() << "  Exported to:" << fullPath;
    }

    qDebug() << "Export complete - New:" << exportedCount << "Overwritten:" << overwrittenCount;
    qDebug() << "Footprint library exported to:" << filePath;

    return true;
}

QString ExporterFootprint::generateHeader(const QString& libName) const {
    return QString(
               "(kicad_pcb (version 20221018) (generator easyeda2kicad)\n"
               "  (version 6)\n"
               "  (generator \"EasyKiConverter\")\n"
               "  (name \"%1\")\n\n")
        .arg(libName);
}

QString ExporterFootprint::generateFootprintContent(const FootprintData& footprintData,
                                                    const QString& model3DPath) const {
    QString content;

    content += QString("(footprint easykiconverter:%1\n").arg(footprintData.info().name);
    content += "  (version 20221018)\n";

    bool isThroughHole = false;
    for (const FootprintPad& pad : footprintData.pads()) {
        if (pad.holeRadius > 0) {
            isThroughHole = true;
            break;
        }
    }

    if (isThroughHole) {
        content += "  (attr through_hole)\n";
    } else {
        content += "  (attr smd)\n";
    }

    double yLow = 0;
    double yHigh = 0;
    if (!footprintData.pads().isEmpty()) {
        yLow = footprintData.pads().first().centerY;
        yHigh = footprintData.pads().first().centerY;
        for (const FootprintPad& pad : footprintData.pads()) {
            if (pad.centerY < yLow)
                yLow = pad.centerY;
            if (pad.centerY > yHigh)
                yHigh = pad.centerY;
        }
    }

    double bboxX = footprintData.bbox().x;
    double bboxY = footprintData.bbox().y;

    content += QString("  (fp_text reference REF** (at 0 %1) (layer F.SilkS)\n")
                   .arg(m_graphicsGenerator.pxToMm(yLow - bboxY - 4));
    content += "    (effects (font (size 1 1) (thickness 0.15)))\n";
    content += "  )\n";

    content += QString("  (fp_text value %1 (at 0 %2) (layer F.Fab)\n")
                   .arg(footprintData.info().name)
                   .arg(m_graphicsGenerator.pxToMm(yHigh - bboxY + 4));
    content += "    (effects (font (size 1 1) (thickness 0.15)))\n";
    content += "  )\n";

    content += "  (fp_text user %R (at 0 0) (layer F.Fab)\n";
    content += "    (effects (font (size 1 1) (thickness 0.15)))\n";
    content += "  )\n";

    for (const FootprintTrack& track : footprintData.tracks()) {
        content += m_graphicsGenerator.generateTrack(track, bboxX, bboxY);
    }
    for (const FootprintRectangle& rect : footprintData.rectangles()) {
        content += m_graphicsGenerator.generateRectangle(rect, bboxX, bboxY);
    }

    for (const FootprintPad& pad : footprintData.pads()) {
        content += m_graphicsGenerator.generatePad(pad, bboxX, bboxY);
    }

    for (const FootprintHole& hole : footprintData.holes()) {
        content += m_graphicsGenerator.generateHole(hole, bboxX, bboxY);
    }

    for (const FootprintCircle& circle : footprintData.circles()) {
        content += m_graphicsGenerator.generateCircle(circle, bboxX, bboxY);
    }

    for (const FootprintArc& arc : footprintData.arcs()) {
        content += m_graphicsGenerator.generateArc(arc, bboxX, bboxY);
    }

    for (const FootprintText& text : footprintData.texts()) {
        content += m_graphicsGenerator.generateText(text, bboxX, bboxY);
    }

    bool hasCourtYard = false;
    for (const FootprintSolidRegion& region : footprintData.solidRegions()) {
        QString regionContent = m_graphicsGenerator.generateSolidRegion(region, bboxX, bboxY);
        content += regionContent;
        if (region.layerId == 99) {
            hasCourtYard = true;
        }
    }

    if (!hasCourtYard && footprintData.bbox().width > 0 && footprintData.bbox().height > 0) {
        content += m_graphicsGenerator.generateCourtyardFromBBox(footprintData.bbox(), bboxX, bboxY);
        qWarning() << "Warning: No courtyard found, generated from BBox";
    }

    if (!footprintData.model3D().name().isEmpty() || !model3DPath.isEmpty()) {
        content += m_graphicsGenerator.generateModel3D(
            footprintData.model3D(), bboxX, bboxY, model3DPath, footprintData.info().type);
    }

    content += ")\n";

    return content;
}

QString ExporterFootprint::generateFootprintContent(const FootprintData& footprintData,
                                                    const QString& model3DWrlPath,
                                                    const QString& model3DStepPath) const {
    QString content;

    content += QString("(footprint easykiconverter:%1\n").arg(footprintData.info().name);
    content += "  (version 20221018)\n";

    bool isThroughHole = false;
    for (const FootprintPad& pad : footprintData.pads()) {
        if (pad.holeRadius > 0) {
            isThroughHole = true;
            break;
        }
    }

    if (isThroughHole) {
        content += "\t(attr through_hole)\n";
    } else {
        content += "\t(attr smd)\n";
    }

    double yLow = 0;
    double yHigh = 0;
    if (!footprintData.pads().isEmpty()) {
        yLow = footprintData.pads().first().centerY;
        yHigh = footprintData.pads().first().centerY;
        for (const FootprintPad& pad : footprintData.pads()) {
            if (pad.centerY < yLow)
                yLow = pad.centerY;
            if (pad.centerY > yHigh)
                yHigh = pad.centerY;
        }
    }

    double bboxX = footprintData.bbox().x;
    double bboxY = footprintData.bbox().y;

    content += QString("\t(fp_text reference REF** (at 0 %1) (layer F.SilkS)\n")
                   .arg(m_graphicsGenerator.pxToMm(yLow - bboxY - 4));
    content += "\t\t(effects (font (size 1 1) (thickness 0.15)))\n";
    content += "\t)\n";

    content += QString("\t(fp_text value %1 (at 0 %2) (layer F.Fab)\n")
                   .arg(footprintData.info().name)
                   .arg(m_graphicsGenerator.pxToMm(yHigh - bboxY + 4));
    content += "\t\t(effects (font (size 1 1) (thickness 0.15)))\n";
    content += "\t)\n";

    content += "\t(fp_text user %R (at 0 0) (layer F.Fab)\n";
    content += "\t\t(effects (font (size 1 1) (thickness 0.15)))\n";
    content += "\t)\n";

    for (const FootprintTrack& track : footprintData.tracks()) {
        content += m_graphicsGenerator.generateTrack(track, bboxX, bboxY);
    }
    for (const FootprintRectangle& rect : footprintData.rectangles()) {
        content += m_graphicsGenerator.generateRectangle(rect, bboxX, bboxY);
    }

    for (const FootprintPad& pad : footprintData.pads()) {
        content += m_graphicsGenerator.generatePad(pad, bboxX, bboxY);
    }

    for (const FootprintHole& hole : footprintData.holes()) {
        content += m_graphicsGenerator.generateHole(hole, bboxX, bboxY);
    }

    for (const FootprintCircle& circle : footprintData.circles()) {
        content += m_graphicsGenerator.generateCircle(circle, bboxX, bboxY);
    }

    for (const FootprintArc& arc : footprintData.arcs()) {
        content += m_graphicsGenerator.generateArc(arc, bboxX, bboxY);
    }

    for (const FootprintText& text : footprintData.texts()) {
        content += m_graphicsGenerator.generateText(text, bboxX, bboxY);
    }

    bool hasCourtYard = false;
    for (const FootprintSolidRegion& region : footprintData.solidRegions()) {
        QString regionContent = m_graphicsGenerator.generateSolidRegion(region, bboxX, bboxY);
        content += regionContent;
        if (region.layerId == 99) {
            hasCourtYard = true;
        }
    }

    if (!hasCourtYard && footprintData.bbox().width > 0 && footprintData.bbox().height > 0) {
        content += m_graphicsGenerator.generateCourtyardFromBBox(footprintData.bbox(), bboxX, bboxY);
        qWarning() << "Warning: No courtyard found, generated from BBox";
    }

    if (!footprintData.model3D().name().isEmpty() || !model3DWrlPath.isEmpty() || !model3DStepPath.isEmpty()) {
        if (!model3DWrlPath.isEmpty()) {
            content += m_graphicsGenerator.generateModel3D(
                footprintData.model3D(), bboxX, bboxY, model3DWrlPath, footprintData.info().type);
        }

        if (!model3DStepPath.isEmpty()) {
            content += m_graphicsGenerator.generateModel3D(
                footprintData.model3D(), bboxX, bboxY, model3DStepPath, footprintData.info().type);
        }
    }

    content += ")\n";
    return content;
}

}  // namespace EasyKiConverter
