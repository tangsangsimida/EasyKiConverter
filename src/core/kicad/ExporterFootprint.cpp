#include "ExporterFootprint.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QFileInfo>
#include "src/core/utils/GeometryUtils.h"

namespace EasyKiConverter {

ExporterFootprint::ExporterFootprint(QObject *parent)
    : QObject(parent)
    , m_kicadVersion(KicadVersion::V6)
{
}

ExporterFootprint::~ExporterFootprint()
{
}

void ExporterFootprint::setKicadVersion(KicadVersion version)
{
    m_kicadVersion = version;
}

ExporterFootprint::KicadVersion ExporterFootprint::getKicadVersion() const
{
    return m_kicadVersion;
}

bool ExporterFootprint::exportFootprint(const FootprintData &footprintData, const QString &filePath, const QString &model3DPath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // Generate footprint content with 3D model path
    QString content = generateFootprintContent(footprintData, model3DPath);

    out << content;
    file.flush();
    file.close();

    qDebug() << "Footprint exported to:" << filePath;
    return true;
}

bool ExporterFootprint::exportFootprintLibrary(const QList<FootprintData> &footprints, const QString &libName, const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // Generate header
    out << generateHeader(libName);

    // Generate all footprints
    for (const FootprintData &footprint : footprints) {
        out << generateFootprintContent(footprint);
    }

    // Generate footer (KiCad 6.x format)
    out << "\n";

    file.close();

    qDebug() << "Footprint library exported to:" << filePath;
    return true;
}

QString ExporterFootprint::generateHeader(const QString &libName) const
{
    // KiCad 6.x format
    return QString("(kicad_pcb (version 20221018) (generator easyeda2kicad)\n"
                   "  (version 6)\n"
                   "  (generator \"EasyKiConverter\")\n"
                   "  (name \"%1\")\n\n").arg(libName);
}

QString ExporterFootprint::generateFootprintContent(const FootprintData &footprintData, const QString &model3DPath) const
{
    QString content;

    // KiCad 6.x format - use module syntax (compatible with KiCad 6+)
    content += QString("(module easyeda2kicad:%1 (layer F.Cu) (tedit 5DC5F6A4)\n").arg(footprintData.info().name);

    // 判断是否为通孔器件：如果有焊盘有孔（holeRadius > 0），则为通孔
    bool isThroughHole = false;
    for (const FootprintPad &pad : footprintData.pads()) {
        if (pad.holeRadius > 0) {
            isThroughHole = true;
            break;
        }
    }

    // 设置正确的属性
    if (isThroughHole) {
        content += "\t(attr through_hole)\n";
    } else {
        content += "\t(attr smd)\n";
    }

    // 如果有自定义类型，也保留（但不应与 through_hole/smd 冲突）
    if (!footprintData.info().type.isEmpty() && footprintData.info().type != "through_hole" && footprintData.info().type != "smd") {
        content += QString("\t(attr %1)\n").arg(footprintData.info().type);
    }

    // Calculate Y positions for reference and value text
    double yLow = 0;
    double yHigh = 0;
    if (!footprintData.pads().isEmpty()) {
        yLow = footprintData.pads().first().centerY;
        yHigh = footprintData.pads().first().centerY;
        for (const FootprintPad &pad : footprintData.pads()) {
            if (pad.centerY < yLow) yLow = pad.centerY;
            if (pad.centerY > yHigh) yHigh = pad.centerY;
        }
    }

    // Get bounding box offset
    double bboxX = footprintData.bbox().x;
    double bboxY = footprintData.bbox().y;

    // Reference text
    content += QString("\t(fp_text reference REF** (at 0 %1) (layer F.SilkS)\n").arg(pxToMm(yLow - bboxY - 4));
    content += "\t\t(effects (font (size 1 1) (thickness 0.15)))\n";
    content += "\t)\n";

    // Value text
    content += QString("\t(fp_text value %1 (at 0 %2) (layer F.Fab)\n").arg(footprintData.info().name).arg(pxToMm(yHigh - bboxY + 4));
    content += "\t\t(effects (font (size 1 1) (thickness 0.15)))\n";
    content += "\t)\n";

    // User reference (Fab layer)
    content += "\t(fp_text user %R (at 0 0) (layer F.Fab)\n";
    content += "\t\t(effects (font (size 1 1) (thickness 0.15)))\n";
    content += "\t)\n";

    // Generate tracks and rectangles
    for (const FootprintTrack &track : footprintData.tracks()) {
        content += generateTrack(track, bboxX, bboxY);
    }
    for (const FootprintRectangle &rect : footprintData.rectangles()) {
        content += generateRectangle(rect, bboxX, bboxY);
    }

    // Generate pads
    for (const FootprintPad &pad : footprintData.pads()) {
        content += generatePad(pad, bboxX, bboxY);
    }

    // Generate holes
    for (const FootprintHole &hole : footprintData.holes()) {
        content += generateHole(hole, bboxX, bboxY);
    }

    // Generate circles
    for (const FootprintCircle &circle : footprintData.circles()) {
        content += generateCircle(circle, bboxX, bboxY);
    }

    // Generate arcs
    for (const FootprintArc &arc : footprintData.arcs()) {
        content += generateArc(arc, bboxX, bboxY);
    }

    // Generate texts
    for (const FootprintText &text : footprintData.texts()) {
        content += generateText(text, bboxX, bboxY);
    }

    // Generate 3D model reference
        if (!footprintData.model3D().name().isEmpty()) {
            content += generateModel3D(footprintData.model3D(), bboxX, bboxY, model3DPath);
        }
    
        content += ")\n";
        
        return content;
    }
QString ExporterFootprint::generatePad(const FootprintPad &pad, double bboxX, double bboxY) const
{
    QString content;
    double x = pxToMm(pad.centerX - bboxX);
    double y = pxToMm(pad.centerY - bboxY);
    double width = pxToMm(pad.width);
    double height = pxToMm(pad.height);
    double holeRadius = pxToMm(pad.holeRadius);

    // 智能判断焊盘形状：当 width 和 height 相等时使用 circle
    QString kicadShape;
    if (qAbs(width - height) < 1e-3) {
        kicadShape = "circle";
    } else {
        kicadShape = padShapeToKicad(pad.shape);
    }

    QString kicadType = padTypeToKicad(pad.layerId);
    QString layers = padLayersToKicad(pad.layerId);
    
    QString drillStr;
    if (holeRadius > 0) {
        double drillDiameter = holeRadius * 2;
        // 限制为2位小数，避免精度冗余（如 2.30002）
        drillStr = QString(" (drill %1)").arg(drillDiameter, 0, 'f', 2);
    }

    // Process pad number: extract content between parentheses if present
    QString padNumber = pad.number;
    if (padNumber.contains("(") && padNumber.contains(")")) {
        int start = padNumber.indexOf("(");
        int end = padNumber.indexOf(")");
        padNumber = padNumber.mid(start + 1, end - start - 1);
    }

    // Convert rotation to match Python version: if > 180, convert to negative
    double rotation = pad.rotation;
    if (rotation > 180.0) {
        rotation = rotation - 360.0;
    }

    // Ensure minimum size
    width = qMax(width, 0.01);
    height = qMax(height, 0.01);

    // KiCad 6.x format - match Python version exactly
    // Note: No quotes around pad number, use rect instead of custom
    content += QString("\t(pad %1 %2 %3 (at %4 %5 %6) (size %7 %8) (layers %9)%10)\n")
        .arg(padNumber)
        .arg(kicadType)
        .arg(kicadShape)
        .arg(x, 0, 'f', 2)
        .arg(y, 0, 'f', 2)
        .arg(rotation, 0, 'f', 2)
        .arg(width, 0, 'f', 2)
        .arg(height, 0, 'f', 2)
        .arg(layers)
        .arg(drillStr);

    return content;
}

QString ExporterFootprint::generateTrack(const FootprintTrack &track, double bboxX, double bboxY) const
{
    QString content;
    
    // Parse points from the string
    QStringList pointList = track.points.split(" ", Qt::SkipEmptyParts);
    
    // KiCad 5.x format - generate line segments from points
    for (int i = 0; i < pointList.size() - 3; i += 2) {
        double startX = pxToMm(pointList[i].toDouble() - bboxX);
        double startY = pxToMm(pointList[i + 1].toDouble() - bboxY);
        double endX = pxToMm(pointList[i + 2].toDouble() - bboxX);
        double endY = pxToMm(pointList[i + 3].toDouble() - bboxY);
        
        content += QString("\t(fp_line (start %1 %2) (end %3 %4) (layer %5) (width %6))\n")
            .arg(startX, 0, 'f', 2)
            .arg(startY, 0, 'f', 2)
            .arg(endX, 0, 'f', 2)
            .arg(endY, 0, 'f', 2)
            .arg(layerIdToKicad(track.layerId))
            .arg(pxToMm(track.strokeWidth), 0, 'f', 2);
    }

    return content;
}

QString ExporterFootprint::generateHole(const FootprintHole &hole, double bboxX, double bboxY) const
{
    QString content;
    double cx = pxToMm(hole.centerX - bboxX);
    double cy = pxToMm(hole.centerY - bboxY);
    double radius = pxToMm(hole.radius);

    // KiCad 5.x format
    content += QString("\t(pad \"\" thru_hole circle (at %1 %2) (size %3 %3) (drill %3) (layers *.Cu *.Mask))\n")
        .arg(cx, 0, 'f', 2)
        .arg(cy, 0, 'f', 2)
        .arg(radius * 2, 0, 'f', 2);

    return content;
}

QString ExporterFootprint::generateCircle(const FootprintCircle &circle, double bboxX, double bboxY) const
{
    QString content;
    double cx = pxToMm(circle.cx - bboxX);
    double cy = pxToMm(circle.cy - bboxY);
    double radius = pxToMm(circle.radius);

    // KiCad 5.x format
    content += QString("\t(fp_circle (center %1 %2) (end %3 %4) (layer %5) (width %6))\n")
        .arg(cx, 0, 'f', 2)
        .arg(cy, 0, 'f', 2)
        .arg(cx + radius, 0, 'f', 2)
        .arg(cy, 0, 'f', 2)
        .arg(layerIdToKicad(circle.layerId))
        .arg(pxToMm(circle.strokeWidth), 0, 'f', 2);

    return content;
}
QString ExporterFootprint::generateRectangle(const FootprintRectangle &rectangle, double bboxX, double bboxY) const
{
    QString content;
    double x = pxToMm(rectangle.x - bboxX);
    double y = pxToMm(rectangle.y - bboxY);
    double width = pxToMm(rectangle.width);
    double height = pxToMm(rectangle.height);
    QString layer = layerIdToKicad(rectangle.layerId);
    double strokeWidth = pxToMm(rectangle.strokeWidth);

    // KiCad 5.x format - 生成完整的矩形外框（4条线）
    // 上边
    content += QString("\t(fp_line (start %1 %2) (end %3 %2) (layer %4) (width %5))\n")
        .arg(x, 0, 'f', 2)
        .arg(y, 0, 'f', 2)
        .arg(x + width, 0, 'f', 2)
        .arg(layer)
        .arg(strokeWidth, 0, 'f', 2);
    // 右边
    content += QString("\t(fp_line (start %1 %2) (end %1 %3) (layer %4) (width %5))\n")
        .arg(x + width, 0, 'f', 2)
        .arg(y, 0, 'f', 2)
        .arg(y + height, 0, 'f', 2)
        .arg(layer)
        .arg(strokeWidth, 0, 'f', 2);
    // 下边
    content += QString("\t(fp_line (start %1 %2) (end %3 %2) (layer %4) (width %5))\n")
        .arg(x, 0, 'f', 2)
        .arg(y + height, 0, 'f', 2)
        .arg(x + width, 0, 'f', 2)
        .arg(layer)
        .arg(strokeWidth, 0, 'f', 2);
    // 左边
    content += QString("\t(fp_line (start %1 %2) (end %1 %3) (layer %4) (width %5))\n")
        .arg(x, 0, 'f', 2)
        .arg(y, 0, 'f', 2)
        .arg(y + height, 0, 'f', 2)
        .arg(layer)
        .arg(strokeWidth, 0, 'f', 2);

    return content;
}

QString ExporterFootprint::generateArc(const FootprintArc &arc, double bboxX, double bboxY) const
{
    QString content;
    
    // Parse path string to extract arc points (simplified - just use first and last points)
    QStringList pointList = arc.path.split(" ", Qt::SkipEmptyParts);
    
    // KiCad 5.x format (simplified - just a line for now)
    if (pointList.size() >= 4) {
        bool ok1, ok2, ok3, ok4;
        double startX = pxToMm(pointList[0].toDouble(&ok1) - bboxX);
        double startY = pxToMm(pointList[1].toDouble(&ok2) - bboxY);
        double endX = pxToMm(pointList[pointList.size() - 2].toDouble(&ok3) - bboxX);
        double endY = pxToMm(pointList[pointList.size() - 1].toDouble(&ok4) - bboxY);
        
        if (ok1 && ok2 && ok3 && ok4) {
            content += QString("\t(fp_line (start %1 %2) (end %3 %4) (layer %5) (width %6))\n")
                .arg(startX, 0, 'f', 2)
                .arg(startY, 0, 'f', 2)
                .arg(endX, 0, 'f', 2)
                .arg(endY, 0, 'f', 2)
                .arg(layerIdToKicad(arc.layerId))
                .arg(pxToMm(arc.strokeWidth), 0, 'f', 2);
        }
    }

    return content;
}

QString ExporterFootprint::generateText(const FootprintText &text, double bboxX, double bboxY) const
{
    QString content;
    double x = pxToMm(text.centerX - bboxX);
    double y = pxToMm(text.centerY - bboxY);
    
    // KiCad 5.x format
    content += QString("\t(fp_text user \"%1\" (at %2 %3 %4) (layer %5)%6\n")
        .arg(text.text)
        .arg(x, 0, 'f', 2)
        .arg(y, 0, 'f', 2)
        .arg(double(text.rotation), 0, 'f', 2)
        .arg(layerIdToKicad(text.layerId));
    
    if (!text.isDisplayed) {
        content += "\n\t\t(effects (font (size 1 1) (thickness 0.15)) (justify left))";
    } else {
        content += "\n\t\t(effects (font (size 1 1) (thickness 0.15)) (justify left))";
    }
    content += "\n\t)\n";

    return content;
}

QString ExporterFootprint::generateModel3D(const Model3DData &model3D, double bboxX, double bboxY, const QString &model3DPath) const
{
    QString content;

    // Use the provided 3D model path if available, otherwise use the model name
    QString finalPath = model3DPath.isEmpty() ? model3D.name() : model3DPath;

    // KiCad 6.x format - use absolute path (match Python version)
    content += QString("\t(model \"%1\"\n").arg(finalPath);
    content += QString("\t\t(offset (xyz %1 %2 %3))\n")
        .arg(pxToMm(model3D.translation().x - bboxX), 0, 'f', 3)
        .arg(pxToMm(model3D.translation().y - bboxY), 0, 'f', 3)
        .arg(pxToMm(model3D.translation().z), 0, 'f', 3);
    content += "\t\t(scale (xyz 1 1 1))\n";
    content += QString("\t\t(rotate (xyz %1 %2 %3))\n")
        .arg(model3D.rotation().x, 0, 'f', 0)
        .arg(model3D.rotation().y, 0, 'f', 0)
        .arg(model3D.rotation().z, 0, 'f', 0);
    content += "\t)\n";

    return content;
}
double ExporterFootprint::pxToMm(double px) const
{
    return GeometryUtils::convertToMm(px);
}

QString ExporterFootprint::padShapeToKicad(const QString &shape) const
{
    // Convert to lowercase for case-insensitive comparison
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
        return "custom"; // Default to custom shape
    }
}

QString ExporterFootprint::padTypeToKicad(int layerId) const
{
    // Simplified layer mapping
    if (layerId == 1) {
        return "smd"; // Top layer
    } else if (layerId == 2) {
        return "smd"; // Bottom layer
    } else {
        return "thru_hole"; // Through hole
    }
}

QString ExporterFootprint::padLayersToKicad(int layerId) const
{
    // Layer mapping based on Python version
    switch (layerId) {
        case 1:
            return "F.Cu F.Paste F.Mask"; // Top SMD
        case 2:
            return "B.Cu B.Paste B.Mask"; // Bottom SMD
        case 3:
            return "F.SilkS";
        case 11:
            return "*.Cu *.Mask"; // Through hole - 不包含 Paste 层
        case 13:
            return "F.Fab";
        case 15:
            return "Dwgs.User";
        default:
            return "*.Cu *.Mask";
    }
}

QString ExporterFootprint::layerIdToKicad(int layerId) const
{
    // Layer mapping for graphical elements
    switch (layerId) {
        case 1:
            return "F.Cu";
        case 2:
            return "B.Cu";
        case 3:
            return "F.SilkS";
        case 11:
            return "*.Cu";
        case 13:
            return "F.Fab";
        case 15:
            return "Dwgs.User";
        default:
            return "F.Fab";
    }
}

} // namespace EasyKiConverter