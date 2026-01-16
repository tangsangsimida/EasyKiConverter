#include "ExporterFootprint.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include "src/core/utils/GeometryUtils.h"

namespace EasyKiConverter {

ExporterFootprint::ExporterFootprint(QObject *parent)
    : QObject(parent)
{
}

ExporterFootprint::~ExporterFootprint()
{
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
    qDebug() << "=== Export Footprint Library ===";
    qDebug() << "Library name:" << libName;
    qDebug() << "Output path:" << filePath;
    qDebug() << "Footprint count:" << footprints.count();

    // filePath 应该是指向 .pretty 文件夹的路径
    QDir libDir(filePath);

    // 如果文件夹不存在，创建它
    if (!libDir.exists()) {
        qDebug() << "Creating footprint library directory:" << filePath;
        if (!libDir.mkpath(".")) {
            qWarning() << "Failed to create footprint library directory:" << filePath;
            return false;
        }
    }

    // 读取现有的封装文件名
    QSet<QString> existingFootprintNames;
    QStringList existingFiles = libDir.entryList(QStringList("*.kicad_mod"), QDir::Files);
    for (const QString &fileName : existingFiles) {
        // 从文件名中提取封装名称（去掉 .kicad_mod 后缀）
        QString footprintName = fileName;
        footprintName.remove(".kicad_mod");
        existingFootprintNames.insert(footprintName);
        qDebug() << "Found existing footprint:" << footprintName;
    }

    qDebug() << "Existing footprints count:" << existingFootprintNames.count();

    // 导出每个封装
    int exportedCount = 0;
    int overwrittenCount = 0;
    for (const FootprintData &footprint : footprints) {
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

        // 生成封装内容
        QString content = generateFootprintContent(footprint);

        // 写入文件
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
    // KiCad只允许以下属性值：through_hole、smd、virtual、board_only、exclude_from_pos_files、exclude_from_bom、allow_solder_mask_bridges
    if (isThroughHole) {
        content += "\t(attr through_hole)\n";
    } else {
        content += "\t(attr smd)\n";
    }

    // KiCad的attr属性只允许特定的值，不能随意添加自定义类型
    // 如果需要额外的属性，必须使用KiCad支持的属性名
    // 这里我们忽略EasyEDA的自定义类型，只使用through_hole或smd
    // 如果将来需要支持其他属性（如virtual、board_only等），需要根据实际需求添加

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

    // Get bounding box center offset (use center as origin instead of top-left corner)
    double bboxX = footprintData.bbox().x + footprintData.bbox().width / 2;
    double bboxY = footprintData.bbox().y + footprintData.bbox().height / 2;

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
    
        // 生成所有图形元素（tracks + rectangles）
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

    // Generate circles (非丝印层)
    for (const FootprintCircle &circle : footprintData.circles()) {
        if (circle.layerId != 3 && circle.layerId != 4) {
            content += generateCircle(circle, bboxX, bboxY);
        }
    }

    // Generate arcs (非丝印层)
    for (const FootprintArc &arc : footprintData.arcs()) {
        if (arc.layerId != 3 && arc.layerId != 4) {
            content += generateArc(arc, bboxX, bboxY);
        }
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
    }QString ExporterFootprint::generatePad(const FootprintPad &pad, double bboxX, double bboxY) const
{
    QString content;
    double x = pxToMm(pad.centerX - bboxX);
    double y = pxToMm(pad.centerY - bboxY);
    double width = pxToMm(pad.width);
    double height = pxToMm(pad.height);
    double holeRadius = pxToMm(pad.holeRadius);

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

    // Check if this is a custom polygon pad
    bool isCustomShape = (padShapeToKicad(pad.shape) == "custom");
    QString polygonStr;

    if (isCustomShape) {
        // Parse points for custom polygon
        QStringList pointList = pad.points.split(" ", Qt::SkipEmptyParts);

        if (pointList.isEmpty()) {
            qWarning() << "Pad" << pad.id << "is a custom polygon, but has no points defined";
        } else {
            // Set the pad width and height to the smallest value allowed by KiCad
            // KiCad tries to draw a pad that forms the base of the polygon,
            // but this is often unnecessary and should be disabled
            width = 0.005;
            height = 0.005;

            // The points of the polygon always seem to correspond to coordinates when orientation=0
            rotation = 0.0;

            // Generate polygon with coordinates relative to the base pad's position
            // Convert points from pixels to mm and make them relative to pad position
            QString path;
            for (int i = 0; i < pointList.size(); i += 2) {
                if (i + 1 < pointList.size()) {
                    double px = pointList[i].toDouble();
                    double py = pointList[i + 1].toDouble();

                    // Convert to mm and make relative to pad position
                    double relX = pxToMm(px - bboxX) - x;
                    double relY = pxToMm(py - bboxY) - y;

                    path += QString("(xy %1 %2) ").arg(relX, 0, 'f', 2).arg(relY, 0, 'f', 2);
                }
            }

            polygonStr = QString("\n\t\t(primitives \n\t\t\t(gr_poly \n\t\t\t\t(pts %1) \n\t\t\t\t(width 0.1) \n\t\t)\n\t)\n\t").arg(path);
        }
    } else {
        // 智能判断焊盘形状：当 width 和 height 相等时使用 circle
        width = qMax(width, 0.01);
        height = qMax(height, 0.01);
    }

    QString kicadShape;
    if (isCustomShape) {
        kicadShape = "custom";
    } else if (qAbs(width - height) < 1e-3) {
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

    // KiCad 6.x format - match Python version exactly
    // Note: No quotes around pad number
    content += QString("\t(pad %1 %2 %3 (at %4 %5 %6) (size %7 %8) (layers %9)%10%11)\n")
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

    // Parse SVG arc path: "M startX startY A radiusX radiusY x_axis_rotation large_arc_flag sweep_flag endX endY"
    QString arcPath = arc.path;
    arcPath.replace(",", " ").replace("M ", "M").replace("A ", "A");

    // Split the path to extract components
    QStringList pathParts = arcPath.split("A", Qt::SkipEmptyParts);
    if (pathParts.size() < 2) {
        qWarning() << "Invalid arc path format:" << arc.path;
        return content;
    }

    // Extract start point
    QString startPart = pathParts[0].mid(1); // Remove "M"
    QStringList startCoords = startPart.split(" ", Qt::SkipEmptyParts);
    if (startCoords.size() < 2) {
        qWarning() << "Invalid start point in arc path:" << arc.path;
        return content;
    }

    double startX = pxToMm(startCoords[0].toDouble() - bboxX);
    double startY = pxToMm(startCoords[1].toDouble() - bboxY);

    // Extract arc parameters
    QString arcParams = pathParts[1].replace("  ", " ");
    QStringList paramParts = arcParams.split(" ", Qt::SkipEmptyParts);
    if (paramParts.size() < 7) {
        qWarning() << "Invalid arc parameters in arc path:" << arc.path;
        return content;
    }

    double svgRx = pxToMm(paramParts[0].toDouble());
    double svgRy = pxToMm(paramParts[1].toDouble());
    double xAxisRotation = paramParts[2].toDouble();
    bool largeArcFlag = (paramParts[3] == "1");
    bool sweepFlag = (paramParts[4] == "1");
    double endX = pxToMm(paramParts[5].toDouble() - bboxX);
    double endY = pxToMm(paramParts[6].toDouble() - bboxY);

    // Compute arc center and angle extent
    double centerX, centerY, angleExtent;
    if (svgRy != 0.0) {
        GeometryUtils::computeArc(startX, startY, svgRx, svgRy, xAxisRotation,
                                  largeArcFlag, sweepFlag, endX, endY,
                                  centerX, centerY, angleExtent);
    } else {
        // Degenerate case: Y radius is zero
        centerX = 0.0;
        centerY = 0.0;
        angleExtent = 0.0;
    }

    // KiCad format: (fp_arc (start cx cy) (end endX endY) (angle angle) (layer layer) (width width))
    content += QString("\t(fp_arc (start %1 %2) (end %3 %4) (angle %5) (layer %6) (width %7))\n")
        .arg(centerX, 0, 'f', 2)
        .arg(centerY, 0, 'f', 2)
        .arg(endX, 0, 'f', 2)
        .arg(endY, 0, 'f', 2)
        .arg(angleExtent, 0, 'f', 2)
        .arg(layerIdToKicad(arc.layerId))
        .arg(pxToMm(arc.strokeWidth), 0, 'f', 2);

    return content;
}

QString ExporterFootprint::generateText(const FootprintText &text, double bboxX, double bboxY) const
{
    QString content;
    double x = pxToMm(text.centerX - bboxX);
    double y = pxToMm(text.centerY - bboxY);

    // Get the layer name
    QString layer = layerIdToKicad(text.layerId);

    // If text type is "N", replace .SilkS with .Fab (match Python version)
    if (text.type == "N") {
        layer = layer.replace(".SilkS", ".Fab");
    }

    // Check if this is a bottom layer text (starts with "B")
    bool isBottomLayer = layer.startsWith("B");
    QString mirrorStr = isBottomLayer ? " mirror" : "";

    // Check if text should be hidden
    QString displayStr = text.isDisplayed ? "" : " hide";

    // KiCad format - match Python version
    content += QString("\t(fp_text user %1 (at %2 %3 %4) (layer %5)%6\n")
        .arg(text.text)
        .arg(x, 0, 'f', 2)
        .arg(y, 0, 'f', 2)
        .arg(double(text.rotation), 0, 'f', 2)
        .arg(layer)
        .arg(displayStr);

    // Font effects
    double fontSize = pxToMm(text.fontSize);
    double thickness = pxToMm(text.strokeWidth);
    fontSize = qMax(fontSize, 1.0);  // Ensure minimum font size
    thickness = qMax(thickness, 0.01);  // Ensure minimum thickness

    content += QString("\t\t(effects (font (size %1 %2) (thickness %3)) (justify left%4))\n")
        .arg(fontSize, 0, 'f', 2)
        .arg(fontSize, 0, 'f', 2)
        .arg(thickness, 0, 'f', 2)
        .arg(mirrorStr);

    content += "\t)\n";

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
    // Layer mapping for graphical elements - matched with Python version
    switch (layerId) {
        case 1:
            return "F.Cu";           // Top Layer
        case 2:
            return "B.Cu";           // Bottom Layer
        case 3:
            return "F.SilkS";        // Top Silkscreen
        case 4:
            return "B.SilkS";        // Bottom Silkscreen
        case 5:
            return "F.Paste";        // Top Solder Paste
        case 6:
            return "B.Paste";        // Bottom Solder Paste
        case 7:
            return "F.Mask";         // Top Solder Mask
        case 8:
            return "B.Mask";         // Bottom Solder Mask
        case 9:
            return "F.Cu";           // Top Layer (alternative)
        case 10:
            return "Edge.Cuts";      // Board Outline (板框层)
        case 11:
            return "Edge.Cuts";      // Board Outline (板框层)
        case 12:
            return "Cmts.User";      // Comments (注释层)
        case 13:
            return "F.Fab";          // Top Fabrication
        case 14:
            return "B.Fab";          // Bottom Fabrication
        case 15:
            return "Dwgs.User";      // Documentation
        case 20:
            return "Cmts.User";      // Comments (alternative)
        case 21:
            return "Eco1.User";      // Eco1
        case 22:
            return "Eco2.User";      // Eco2
        case 100:
            return "F.Fab";          // Lead Shape Layer (引脚形状层) - 同时也会生成 F.SilkS 版本
        case 101:
            return "F.SilkS";        // Component Polarity Layer (极性标记层)
        default:
            // 默认使用 F.Fab 层，避免丢失数据
            qWarning() << "Unknown layer ID:" << layerId << ", defaulting to F.Fab";
            return "F.Fab";
    }
}

} // namespace EasyKiConverter