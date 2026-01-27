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


    content += QString("  (fp_text reference REF** (at 0 %1) (layer F.SilkS)\n").arg(pxToMm(yLow - bboxY - 4));
    content += "    (effects (font (size 1 1) (thickness 0.15)))\n";
    content += "  )\n";


    content += QString("  (fp_text value %1 (at 0 %2) (layer F.Fab)\n")
                   .arg(footprintData.info().name)
                   .arg(pxToMm(yHigh - bboxY + 4));
    content += "    (effects (font (size 1 1) (thickness 0.15)))\n";
    content += "  )\n";


    content += "  (fp_text user %R (at 0 0) (layer F.Fab)\n";
    content += "    (effects (font (size 1 1) (thickness 0.15)))\n";
    content += "  )\n";


    for (const FootprintTrack& track : footprintData.tracks()) {
        content += generateTrack(track, bboxX, bboxY);
    }
    for (const FootprintRectangle& rect : footprintData.rectangles()) {
        content += generateRectangle(rect, bboxX, bboxY);
    }


    for (const FootprintPad& pad : footprintData.pads()) {
        content += generatePad(pad, bboxX, bboxY);
    }


    for (const FootprintHole& hole : footprintData.holes()) {
        content += generateHole(hole, bboxX, bboxY);
    }


    for (const FootprintCircle& circle : footprintData.circles()) {
        content += generateCircle(circle, bboxX, bboxY);
    }


    for (const FootprintArc& arc : footprintData.arcs()) {
        content += generateArc(arc, bboxX, bboxY);
    }


    for (const FootprintText& text : footprintData.texts()) {
        content += generateText(text, bboxX, bboxY);
    }


    bool hasCourtYard = false;
    for (const FootprintSolidRegion& region : footprintData.solidRegions()) {
        QString regionContent = generateSolidRegion(region, bboxX, bboxY);
        content += regionContent;
        if (region.layerId == 99) {
            hasCourtYard = true;
        }
    }


    if (!hasCourtYard && footprintData.bbox().width > 0 && footprintData.bbox().height > 0) {
        content += generateCourtyardFromBBox(footprintData.bbox(), bboxX, bboxY);
        qWarning() << "Warning: No courtyard found, generated from BBox";
    }


    if (!footprintData.model3D().name().isEmpty()) {
        content += generateModel3D(footprintData.model3D(), bboxX, bboxY, model3DPath, footprintData.info().type);
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


    content += QString("\t(fp_text reference REF** (at 0 %1) (layer F.SilkS)\n").arg(pxToMm(yLow - bboxY - 4));
    content += "\t\t(effects (font (size 1 1) (thickness 0.15)))\n";
    content += "\t)\n";


    content += QString("\t(fp_text value %1 (at 0 %2) (layer F.Fab)\n")
                   .arg(footprintData.info().name)
                   .arg(pxToMm(yHigh - bboxY + 4));
    content += "\t\t(effects (font (size 1 1) (thickness 0.15)))\n";
    content += "\t)\n";


    content += "\t(fp_text user %R (at 0 0) (layer F.Fab)\n";
    content += "\t\t(effects (font (size 1 1) (thickness 0.15)))\n";
    content += "\t)\n";


    for (const FootprintTrack& track : footprintData.tracks()) {
        content += generateTrack(track, bboxX, bboxY);
    }
    for (const FootprintRectangle& rect : footprintData.rectangles()) {
        content += generateRectangle(rect, bboxX, bboxY);
    }


    for (const FootprintPad& pad : footprintData.pads()) {
        content += generatePad(pad, bboxX, bboxY);
    }


    for (const FootprintHole& hole : footprintData.holes()) {
        content += generateHole(hole, bboxX, bboxY);
    }


    for (const FootprintCircle& circle : footprintData.circles()) {
        content += generateCircle(circle, bboxX, bboxY);
    }


    for (const FootprintArc& arc : footprintData.arcs()) {
        content += generateArc(arc, bboxX, bboxY);
    }


    for (const FootprintText& text : footprintData.texts()) {
        content += generateText(text, bboxX, bboxY);
    }


    bool hasCourtYard = false;
    for (const FootprintSolidRegion& region : footprintData.solidRegions()) {
        QString regionContent = generateSolidRegion(region, bboxX, bboxY);
        content += regionContent;
        if (region.layerId == 99) {
            hasCourtYard = true;
        }
    }


    if (!hasCourtYard && footprintData.bbox().width > 0 && footprintData.bbox().height > 0) {
        content += generateCourtyardFromBBox(footprintData.bbox(), bboxX, bboxY);
        qWarning() << "Warning: No courtyard found, generated from BBox";
    }


    if (!footprintData.model3D().name().isEmpty()) {
        if (!model3DWrlPath.isEmpty()) {
            content +=
                generateModel3D(footprintData.model3D(), bboxX, bboxY, model3DWrlPath, footprintData.info().type);
        }

        if (!model3DStepPath.isEmpty()) {
            content +=
                generateModel3D(footprintData.model3D(), bboxX, bboxY, model3DStepPath, footprintData.info().type);
        }
    }

    content += ")\n";
    return content;
}

QString ExporterFootprint::generatePad(const FootprintPad& pad, double bboxX, double bboxY) const {
    QString content;

    double x = pxToMmRounded(pad.centerX - bboxX);
    double y = pxToMmRounded(pad.centerY - bboxY);
    double width = pxToMmRounded(pad.width);
    double height = pxToMmRounded(pad.height);
    double holeRadius = pxToMmRounded(pad.holeRadius);


    QString padNumber = pad.number;
    if (padNumber.contains("(") && padNumber.contains(")")) {
        int start = padNumber.indexOf("(");
        int end = padNumber.indexOf(")");
        padNumber = padNumber.mid(start + 1, end - start - 1);
    }


    double rotation = pad.rotation;
    if (rotation > 180.0) {
        rotation = rotation - 360.0;
    }


    bool isCustomShape = (padShapeToKicad(pad.shape) == "custom");
    QString polygonStr;

    if (isCustomShape) {
        QStringList pointList = pad.points.split(" ", Qt::SkipEmptyParts);

        if (pointList.isEmpty()) {
            qWarning() << "Pad" << pad.id << "is a custom polygon, but has no points defined";
        } else {
            width = 0.005;
            height = 0.005;


            rotation = 0.0;


            QString path;
            for (int i = 0; i < pointList.size(); i += 2) {
                if (i + 1 < pointList.size()) {
                    double px = pointList[i].toDouble();
                    double py = pointList[i + 1].toDouble();


                    double relX = pxToMmRounded(px - bboxX) - x;
                    double relY = pxToMmRounded(py - bboxY) - y;

                    path += QString("(xy %1 %2) ").arg(relX, 0, 'f', 2).arg(relY, 0, 'f', 2);
                }
            }

            polygonStr = QString(
                             "\n    (primitives\n      (gr_poly\n        (pts %1)\n        (width 0.1)\n        (fill "
                             "yes)\n      )\n    )\n  ")
                             .arg(path);
        }
    } else {
        width = qMax(width, 0.01);
        height = qMax(height, 0.01);
    }

    QString kicadShape;
    if (isCustomShape) {
        kicadShape = "custom";
    } else {
        kicadShape = padShapeToKicad(pad.shape);
    }

    QString kicadType = padTypeToKicad(pad.layerId);
    QString layers = padLayersToKicad(pad.layerId);

    QString drillStr;
    if (holeRadius > 0 && pad.holeLength > 0) {
        double holeLengthMm = pxToMmRounded(pad.holeLength);
        double maxDistanceHole = qMax(holeRadius * 2, holeLengthMm);
        double pos0 = height - maxDistanceHole;
        double pos90 = width - maxDistanceHole;
        double maxDistance = qMax(pos0, pos90);


        if (qAbs(maxDistance - pos0) < qAbs(maxDistance - pos90)) {
            drillStr = QString(" (drill oval %1 %2)").arg(holeRadius * 2, 0, 'f', 2).arg(holeLengthMm, 0, 'f', 2);
        } else {
            drillStr = QString(" (drill oval %1 %2)").arg(holeLengthMm, 0, 'f', 2).arg(holeRadius * 2, 0, 'f', 2);
        }
    } else if (holeRadius > 0) {
        double drillDiameter = holeRadius * 2;

        drillStr = QString(" (drill %1)").arg(drillDiameter, 0, 'f', 2);
    }


    content += QString("  (pad %1 %2 %3 (at %4 %5 %6) (size %7 %8) (layers %9)%10%11)\n")
                   .arg(padNumber)
                   .arg(kicadType)
                   .arg(kicadShape)
                   .arg(x, 0, 'f', 2)
                   .arg(y, 0, 'f', 2)
                   .arg(rotation, 0, 'f', 2)
                   .arg(width, 0, 'f', 2)
                   .arg(height, 0, 'f', 2)
                   .arg(layers)
                   .arg(drillStr)
                   .arg(polygonStr);

    return content;
}

QString ExporterFootprint::generateTrack(const FootprintTrack& track, double bboxX, double bboxY) const {
    QString content;


    QStringList pointList = track.points.split(" ", Qt::SkipEmptyParts);


    for (int i = 0; i + 3 < pointList.size(); i += 2) {
        double startX = pxToMmRounded(pointList[i].toDouble() - bboxX);
        double startY = pxToMmRounded(pointList[i + 1].toDouble() - bboxY);
        double endX = pxToMmRounded(pointList[i + 2].toDouble() - bboxX);
        double endY = pxToMmRounded(pointList[i + 3].toDouble() - bboxY);

        content += QString("  (fp_line (start %1 %2) (end %3 %4) (layer %5) (width %6))\n")
                       .arg(startX, 0, 'f', 2)
                       .arg(startY, 0, 'f', 2)
                       .arg(endX, 0, 'f', 2)
                       .arg(endY, 0, 'f', 2)
                       .arg(layerIdToKicad(track.layerId))
                       .arg(pxToMmRounded(track.strokeWidth), 0, 'f', 2);
    }

    return content;
}

QString ExporterFootprint::generateHole(const FootprintHole& hole, double bboxX, double bboxY) const {
    QString content;
    double cx = pxToMmRounded(hole.centerX - bboxX);
    double cy = pxToMmRounded(hole.centerY - bboxY);
    double radius = pxToMmRounded(hole.radius);

    content += QString("  (pad \"\" thru_hole circle (at %1 %2) (size %3 %3) (drill %3) (layers *.Cu *.Mask))\n")
                   .arg(cx, 0, 'f', 2)
                   .arg(cy, 0, 'f', 2)
                   .arg(radius * 2, 0, 'f', 2);

    return content;
}

QString ExporterFootprint::generateCircle(const FootprintCircle& circle, double bboxX, double bboxY) const {
    QString content;
    double cx = pxToMmRounded(circle.cx - bboxX);
    double cy = pxToMmRounded(circle.cy - bboxY);
    double radius = pxToMmRounded(circle.radius);

    content += QString("  (fp_circle (center %1 %2) (end %3 %4) (layer %5) (width %6))\n")
                   .arg(cx, 0, 'f', 2)
                   .arg(cy, 0, 'f', 2)
                   .arg(cx + radius, 0, 'f', 2)
                   .arg(cy, 0, 'f', 2)
                   .arg(layerIdToKicad(circle.layerId))
                   .arg(pxToMm(circle.strokeWidth), 0, 'f', 2);

    return content;
}
QString ExporterFootprint::generateRectangle(const FootprintRectangle& rectangle, double bboxX, double bboxY) const {
    QString content;
    double x = pxToMmRounded(rectangle.x - bboxX);
    double y = pxToMmRounded(rectangle.y - bboxY);
    double width = pxToMmRounded(rectangle.width);
    double height = pxToMmRounded(rectangle.height);
    QString layer = layerIdToKicad(rectangle.layerId);
    double strokeWidth = pxToMmRounded(rectangle.strokeWidth);


    content += QString("  (fp_line (start %1 %2) (end %3 %2) (layer %4) (width %5))\n")
                   .arg(x, 0, 'f', 2)
                   .arg(y, 0, 'f', 2)
                   .arg(x + width, 0, 'f', 2)
                   .arg(layer)
                   .arg(strokeWidth, 0, 'f', 2);

    content += QString("  (fp_line (start %1 %2) (end %1 %3) (layer %4) (width %5))\n")
                   .arg(x + width, 0, 'f', 2)
                   .arg(y, 0, 'f', 2)
                   .arg(y + height, 0, 'f', 2)
                   .arg(layer)
                   .arg(strokeWidth, 0, 'f', 2);

    content += QString("  (fp_line (start %1 %2) (end %3 %2) (layer %4) (width %5))\n")
                   .arg(x, 0, 'f', 2)
                   .arg(y + height, 0, 'f', 2)
                   .arg(x + width, 0, 'f', 2)
                   .arg(layer)
                   .arg(strokeWidth, 0, 'f', 2);

    content += QString("  (fp_line (start %1 %2) (end %1 %3) (layer %4) (width %5))\n")
                   .arg(x, 0, 'f', 2)
                   .arg(y, 0, 'f', 2)
                   .arg(y + height, 0, 'f', 2)
                   .arg(layer)
                   .arg(strokeWidth, 0, 'f', 2);

    return content;
}

QString ExporterFootprint::generateArc(const FootprintArc& arc, double bboxX, double bboxY) const {
    QString content;


    QString arcParam = "M " + arc.path;


    GeometryUtils::SvgArcResult svgArc = GeometryUtils::solveSvgArc(arcParam);


    double cx = pxToMmRounded(svgArc.cx - bboxX);
    double cy = -pxToMmRounded(svgArc.cy - bboxY);


    if (svgArc.rx == svgArc.ry && svgArc.rx > 0) {
        double startAngle = svgArc.startAngle;
        double deltaAngle = svgArc.deltaAngle;
        double step = 180.0;

        if (deltaAngle < 0) {
            startAngle = startAngle + deltaAngle;
            deltaAngle = -deltaAngle;
        }

        while (deltaAngle > 0.1) {
            if (deltaAngle < step) {
                step = deltaAngle;
            }

            svgArc.startAngle = startAngle;
            svgArc.deltaAngle = step;

            GeometryUtils::SvgArcEndpoints pt = GeometryUtils::calcSvgArc(svgArc);

            double kiEndAngle = svgArc.startAngle;
            if (step == 180.0)
                kiEndAngle += 0.1;

            content += QString("  (fp_arc (start %1 %2) (end %3 %4) (angle %5) (layer %6) (width %7))\n")
                           .arg(cx, 0, 'f', 2)
                           .arg(cy, 0, 'f', 2)
                           .arg(pxToMmRounded(pt.x2 - bboxX), 0, 'f', 2)
                           .arg(-pxToMmRounded(pt.y2 - bboxY), 0, 'f', 2)
                           .arg(-kiEndAngle, 0, 'f', 2)
                           .arg(layerIdToKicad(arc.layerId))
                           .arg(pxToMmRounded(arc.strokeWidth), 0, 'f', 2);

            deltaAngle -= step;
            startAngle += step;
        }
    } else {
        QList<GeometryUtils::SvgPoint> points = GeometryUtils::arcToPath(svgArc, false);

        for (int i = 1; i < points.size(); ++i) {
            double x1 = pxToMmRounded(points[i - 1].x - bboxX);
            double y1 = -pxToMmRounded(points[i - 1].y - bboxY);
            double x2 = pxToMmRounded(points[i].x - bboxX);
            double y2 = -pxToMmRounded(points[i].y - bboxY);

            content += QString("  (fp_line (start %1 %2) (end %3 %4) (layer %5) (width %6))\n")
                           .arg(x1, 0, 'f', 2)
                           .arg(y1, 0, 'f', 2)
                           .arg(x2, 0, 'f', 2)
                           .arg(y2, 0, 'f', 2)
                           .arg(layerIdToKicad(arc.layerId))
                           .arg(pxToMmRounded(arc.strokeWidth), 0, 'f', 2);
        }
    }

    return content;
}
QString ExporterFootprint::generateText(const FootprintText& text, double bboxX, double bboxY) const {
    QString content;
    double x = pxToMmRounded(text.centerX - bboxX);
    double y = pxToMmRounded(text.centerY - bboxY);


    QString layer = layerIdToKicad(text.layerId);


    if (text.type == "N") {
        layer = layer.replace(".SilkS", ".Fab");
    }


    QString displayStr = text.isDisplayed ? "" : " hide";

    bool isNonASCII = false;
    for (int i = 0; i < text.text.length(); ++i) {
        if (text.text[i].unicode() > 127) {
            isNonASCII = true;
            break;
        }
    }
    if (isNonASCII && !text.textPath.isEmpty()) {
        qWarning() << "Warning: Converting non-ASCII text to polygon:" << text.text;

        QStringList paths = text.textPath.split("M", Qt::SkipEmptyParts);
        for (const QString& pathStr : paths) {
            if (pathStr.trimmed().isEmpty())
                continue;

            QStringList tokens = pathStr.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
            QList<QPointF> points;
            for (int i = 0; i + 1 < tokens.size(); i += 2) {
                double px = tokens[i].toDouble();
                double py = tokens[i + 1].toDouble();

                double relX = pxToMmRounded(px - bboxX);
                double relY = pxToMmRounded(py - bboxY);
                points.append(QPointF(relX, relY));
            }

            if (points.size() >= 2) {
                for (int i = 1; i < points.size(); ++i) {
                    content += QString("  (fp_line (start %1 %2) (end %3 %4) (layer %5) (width %6))\n")
                                   .arg(points[i - 1].x(), 0, 'f', 2)
                                   .arg(points[i - 1].y(), 0, 'f', 2)
                                   .arg(points[i].x(), 0, 'f', 2)
                                   .arg(points[i].y(), 0, 'f', 2)
                                   .arg(layer)
                                   .arg(pxToMmRounded(text.strokeWidth) * 0.8, 0, 'f', 2);
                }
            }
        }
    } else {
        bool isBottomLayer = layer.startsWith("B");
        QString mirrorStr = isBottomLayer ? " mirror" : "";

        content += QString("  (fp_text user %1 (at %2 %3 %4) (layer %5)%6\n")
                       .arg(text.text)
                       .arg(x, 0, 'f', 2)
                       .arg(y, 0, 'f', 2)
                       .arg(double(text.rotation), 0, 'f', 2)
                       .arg(layer)
                       .arg(displayStr);

        double fontSize = pxToMmRounded(text.fontSize);
        double thickness = pxToMmRounded(text.strokeWidth);
        fontSize = qMax(fontSize, 1.0);
        thickness = qMax(thickness, 0.01);
        content += QString("    (effects (font (size %1 %2) (thickness %3)) (justify left%4))\n")
                       .arg(fontSize, 0, 'f', 2)
                       .arg(fontSize, 0, 'f', 2)
                       .arg(thickness, 0, 'f', 2)
                       .arg(mirrorStr);
        content += "  )\n";
    }

    return content;
}

QString ExporterFootprint::generateModel3D(const Model3DData& model3D,

                                           double bboxX,

                                           double bboxY,

                                           const QString& model3DPath,

                                           const QString& fpType) const {
    QString content;


    QString finalPath = model3DPath.isEmpty() ? model3D.name() : model3DPath;


    double z = pxToMmRounded(model3D.translation().z);

    if (fpType == "smd") {
        z = -z;

    } else {
        z = 0.0;
    }


    double rotX = (360.0 - model3D.rotation().x);

    while (rotX >= 360.0)

        rotX -= 360.0;

    double rotY = (360.0 - model3D.rotation().y);

    while (rotY >= 360.0)

        rotY -= 360.0;

    double rotZ = (360.0 - model3D.rotation().z);

    while (rotZ >= 360.0)

        rotZ -= 360.0;


    // 计算 3D 模型的 offset

    // 检测是否为异常的小值（来自特殊单位的 c_origin）

    // 如果 model3D.translation() 的值远小于 bbox（< 100），说明已经转换为毫米单位

    double finalOffsetX, finalOffsetY;

    if (qAbs(model3D.translation().x) < 100 && qAbs(model3D.translation().y) < 100) {
        // 异常值：translation 已经是毫米单位

        // 对于异常封装，translation 可能是相对于焊盘中心的偏移，而不是相对于 bbox 的

        // 需要计算焊盘中心相对于 bbox 左上角的偏移

        double padsCenterX = bboxX + 3999.5;  // 使用固定的焊盘中心 (4000, 3000) 作为基准

        double padsCenterY = bboxY + 2999.5;


        // 将焊盘中心转换为毫米单位

        double padsCenterX_mm = pxToMmRounded(padsCenterX - bboxX);

        double padsCenterY_mm = pxToMmRounded(padsCenterY - bboxY);


        // 最终偏移 = translation - 焊盘中心偏移

        finalOffsetX = model3D.translation().x - padsCenterX_mm;

        finalOffsetY = -(model3D.translation().y - padsCenterY_mm);  // Y 轴取反

    } else {
        // 正常值：translation 和 bbox 都是像素单位，使用标准的 pxToMm 转换逻辑

        double offsetX = model3D.translation().x - bboxX;

        double offsetY = model3D.translation().y - bboxY;

        finalOffsetX = pxToMmRounded(offsetX);

        finalOffsetY = -pxToMmRounded(offsetY);  // Y 轴取反（正常情况）
    }


    content += QString("  (model \"%1\"\n").arg(finalPath);

    content += QString("    (offset (xyz %1 %2 %3))\n")

                   .arg(finalOffsetX, 0, 'f', 3)

                   .arg(finalOffsetY, 0, 'f', 3)

                   .arg(z, 0, 'f', 3);

    content += "    (scale (xyz 1 1 1))\n";

    content += QString("    (rotate (xyz %1 %2 %3))\n").arg(rotX, 0, 'f', 0).arg(rotY, 0, 'f', 0).arg(rotZ, 0, 'f', 0);

    content += "  )\n";


    return content;
}
double ExporterFootprint::pxToMm(double px) const {
    return GeometryUtils::convertToMm(px);
}

double ExporterFootprint::pxToMmRounded(double px) const {
    return std::floor(GeometryUtils::convertToMm(px) * 100.0) / 100.0;
}

QString ExporterFootprint::generateSolidRegion(const FootprintSolidRegion& region, double bboxX, double bboxY) const {
    QString content;


    bool isCourtYard = (region.layerId == 99);
    QString layer;

    if (isCourtYard) {
        layer = "F.CrtYd";
    } else if (region.layerId == 100) {
        layer = "F.Fab";
    } else if (region.layerId == 101) {
        layer = "F.SilkS";
    } else {
        layer = layerIdToKicad(region.layerId);
    }


    QStringList tokens = region.path.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
    QList<QPointF> points;

    for (int i = 0; i < tokens.size();) {
        QString token = tokens[i].toUpper();

        if (token == "M") {
            if (i + 2 < tokens.size()) {
                double x = tokens[i + 1].toDouble();
                double y = tokens[i + 2].toDouble();
                points.append(QPointF(pxToMmRounded(x - bboxX), pxToMmRounded(y - bboxY)));
                i += 3;
            } else {
                i++;
            }
        } else if (token == "L") {
            if (i + 2 < tokens.size()) {
                double x = tokens[i + 1].toDouble();
                double y = tokens[i + 2].toDouble();
                points.append(QPointF(pxToMmRounded(x - bboxX), pxToMmRounded(y - bboxY)));
                i += 3;
            } else {
                i++;
            }
        } else if (token == "Z") {
            if (!points.isEmpty()) {
                points.append(points.first());
            }
            i++;
        } else {
            i++;
        }
    }


    if (points.size() >= 2) {
        if (isCourtYard || region.isKeepOut) {
            for (int i = 1; i < points.size(); ++i) {
                content += QString("  (fp_line (start %1 %2) (end %3 %4) (layer %5) (width 0.05))\n")
                               .arg(points[i - 1].x(), 0, 'f', 2)
                               .arg(points[i - 1].y(), 0, 'f', 2)
                               .arg(points[i].x(), 0, 'f', 2)
                               .arg(points[i].y(), 0, 'f', 2)
                               .arg(layer);
            }
        } else {
            content += "  (fp_poly\n";
            content += "    (pts";
            for (const QPointF& pt : points) {
                content += QString(" (xy %1 %2)").arg(pt.x(), 0, 'f', 2).arg(pt.y(), 0, 'f', 2);
            }
            content += ")\n";
            content += QString("    (layer %1)\n").arg(layer);
            content += "    (width 0.1)\n";
            if (region.fillStyle == "solid") {
                content += "    (fill solid)\n";
            }
            content += "  )\n";
        }
    }

    return content;
}

QString ExporterFootprint::generateCourtyardFromBBox(const FootprintBBox& bbox, double bboxX, double bboxY) const {
    QString content;


    double x1 = pxToMmRounded(bbox.x - bboxX);
    double y1 = pxToMmRounded(bbox.y - bboxY);
    double x2 = pxToMmRounded(bbox.x + bbox.width - bboxX);
    double y2 = pxToMmRounded(bbox.y + bbox.height - bboxY);


    content += QString("  (fp_line (start %1 %2) (end %3 %2) (layer F.CrtYd) (width 0.05))\n")
                   .arg(x1, 0, 'f', 2)
                   .arg(y1, 0, 'f', 2)
                   .arg(x2, 0, 'f', 2);
    content += QString("  (fp_line (start %3 %2) (end %3 %4) (layer F.CrtYd) (width 0.05))\n")
                   .arg(x2, 0, 'f', 2)
                   .arg(y1, 0, 'f', 2)
                   .arg(y2, 0, 'f', 2);
    content += QString("  (fp_line (start %3 %4) (end %1 %4) (layer F.CrtYd) (width 0.05))\n")
                   .arg(x1, 0, 'f', 2)
                   .arg(x2, 0, 'f', 2)
                   .arg(y2, 0, 'f', 2);
    content += QString("  (fp_line (start %1 %4) (end %1 %2) (layer F.CrtYd) (width 0.05))\n")
                   .arg(x1, 0, 'f', 2)
                   .arg(y1, 0, 'f', 2)
                   .arg(y2, 0, 'f', 2);

    return content;
}

QString ExporterFootprint::padShapeToKicad(const QString& shape) const {
    QString shapeLower = shape.toLower();

    if (shapeLower == "rect" || shapeLower == "rectangle") {
        return "rect";
    } else if (shapeLower == "circle") {
        return "circle";
    } else if (shapeLower == "oval") {
        return "oval";
    } else if (shapeLower == "roundrect") {
        return "roundrect";
    } else if (shapeLower == "trapezoid") {
        return "trapezoid";
    } else {
        return "custom";
    }
}

QString ExporterFootprint::padTypeToKicad(int layerId) const {
    if (layerId == 1) {
        return "smd";
    } else if (layerId == 2) {
        return "smd";
    } else {
        return "thru_hole";
    }
}

QString ExporterFootprint::padLayersToKicad(int layerId) const {
    switch (layerId) {
        case 1:

            return "F.Cu F.Paste F.Mask";
        case 2:

            return "B.Cu B.Paste B.Mask";
        case 3:

            return "F.SilkS";
        case 4:

            return "B.SilkS";
        case 11:


            return "*.Cu *.Mask";
        case 13:

            return "F.Fab";
        case 14:

            return "B.Fab";
        case 15:

            return "Dwgs.User";
        default:

            qWarning() << "Unknown pad layer ID:" << layerId << ", using default thru-hole configuration";
            return "*.Cu *.Mask";
    }
}
QString ExporterFootprint::layerIdToKicad(int layerId) const {
    switch (layerId) {
        case 1:
            return "F.Cu";
        case 2:
            return "B.Cu";
        case 3:
            return "F.SilkS";
        case 4:
            return "B.SilkS";
        case 5:
            return "F.Paste";
        case 6:
            return "B.Paste";
        case 7:
            return "F.Mask";
        case 8:
            return "B.Mask";
        case 9:
            return "F.Cu";
        case 10:
            return "Edge.Cuts";
        case 11:
            return "Edge.Cuts";
        case 12:
            return "Dwgs.User";
        case 13:
            return "F.Fab";
        case 14:
            return "B.Fab";
        case 15:
            return "Dwgs.User";
        case 20:
            return "Cmts.User";
        case 21:
            return "Eco1.User";
        case 22:
            return "Eco2.User";
        case 100:
            return "F.Fab";
        case 101:
            return "F.SilkS";
        default:

            qWarning() << "Unknown layer ID:" << layerId << ", defaulting to F.Fab";
            return "F.Fab";
    }
}

}  // namespace EasyKiConverter
