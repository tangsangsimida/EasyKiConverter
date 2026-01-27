#include "ExporterSymbol.h"

#include "core/utils/GeometryUtils.h"
#include "core/utils/SvgPathParser.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>

namespace EasyKiConverter {

ExporterSymbol::ExporterSymbol(QObject* parent) : QObject(parent) {}

ExporterSymbol::~ExporterSymbol() {}

bool ExporterSymbol::exportSymbol(const SymbolData& symbolData, const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // 生成符号内容（不包含库头，仅单个符号定义�?
    QString content = generateSymbolContent(symbolData, "");

    out << content;
    file.close();

    qDebug() << "Symbol exported to:" << filePath;
    return true;
}

bool ExporterSymbol::exportSymbolLibrary(const QList<SymbolData>& symbols,
                                         const QString& libName,
                                         const QString& filePath,
                                         bool appendMode,
                                         bool updateMode) {
    qDebug() << "=== Export Symbol Library ===";
    qDebug() << "Library name:" << libName;
    qDebug() << "Output path:" << filePath;
    qDebug() << "Symbol count:" << symbols.count();
    qDebug() << "KiCad version: V6";
    qDebug() << "Append mode:" << appendMode;
    qDebug() << "Update mode:" << updateMode;

    QFile file(filePath);
    bool fileExists = file.exists();

    // 如果文件不存在，直接创建新库
    if (!fileExists) {
        qDebug() << "Creating new symbol library...";
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Failed to open file for writing:" << filePath;
            return false;
        }

        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);

        // 生成头部
        out << generateHeader(libName);

        // 生成所有符�?
        int index = 0;
        for (const SymbolData& symbol : symbols) {
            qDebug() << "Exporting symbol" << (++index) << "of" << symbols.count() << ":" << symbol.info().name;
            out << generateSymbolContent(symbol, libName);
        }

        // 生成尾部
        out << ")\n";  // 闭合 kicad_symbol_lib

        file.close();
        qDebug() << "Symbol library created successfully:" << filePath;
        return true;
    }

    // 文件存在，需要处理追加或更新
    qDebug() << "Existing library found, reading content...";

    // 读取现有库内�?
    QString existingContent;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        existingContent = QTextStream(&file).readAll();
        file.close();
        qDebug() << "Read" << existingContent.length() << "bytes from existing library";
    } else {
        qWarning() << "Failed to open existing library for reading:" << filePath;
        return false;
    }

    // 提取现有符号
    QMap<QString, QString> existingSymbols;  // 符号�?-> 符号内容
    QSet<QString> subSymbolNames;            // 属于分体式符号的子符号名�?

    // 使用更可靠的方法来提取符号：手动解析括号匹配
    QStringList lines = existingContent.split('\n');
    int symbolStart = -1;
    QString currentSymbolName;
    int braceCount = 0;
    int parentSymbolDepth = 0;  // 父符号的深度（用于识别子符号�?

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();

        // 检查是否是符号定义的开�?
        if (line.startsWith("(symbol \"")) {
            // 提取符号�?
            int nameStart = line.indexOf("\"") + 1;
            int nameEnd = line.indexOf("\"", nameStart);
            if (nameEnd > nameStart) {
                QString symbolName = line.mid(nameStart, nameEnd - nameStart);

                if (symbolStart >= 0) {
                    // 这是一个嵌套的符号定义（子符号�?
                    subSymbolNames.insert(symbolName);
                    qDebug() << "Found sub-symbol:" << symbolName << "inside parent symbol";
                } else {
                    // 这是一个顶层符号定�?
                    currentSymbolName = symbolName;
                    symbolStart = i;
                    braceCount = 1;
                    parentSymbolDepth = 0;
                }
            }
        } else if (symbolStart >= 0) {
            // 计算括号数量
            for (int j = 0; j < line.length(); ++j) {
                if (line[j] == '(') {
                    braceCount++;
                    // 如果括号深度大于 1，说明我们在父符号内�?
                    if (braceCount > 1)
                        parentSymbolDepth++;
                } else if (line[j] == ')') {
                    braceCount--;
                    if (parentSymbolDepth > 0)
                        parentSymbolDepth--;
                }
            }

            // 当括号数量归零时，符号定义结�?
            if (braceCount == 0) {
                // 只保存顶层符号，不保存子符号
                QString symbolContent;
                for (int k = symbolStart; k <= i; ++k) {
                    symbolContent += lines[k] + "\n";
                }
                existingSymbols[currentSymbolName] = symbolContent;
                qDebug() << "Found existing symbol:" << currentSymbolName << "at lines" << symbolStart << "-" << i;

                // 重置状�?
                symbolStart = -1;
                currentSymbolName.clear();
                parentSymbolDepth = 0;
            }
        }
    }

    qDebug() << "Existing symbols count:" << existingSymbols.count();
    qDebug() << "Sub-symbol names:" << subSymbolNames;

    qDebug() << "Existing symbols count:" << existingSymbols.count();

    // 确定要导出的符号
    QList<SymbolData> symbolsToExport;
    int overwriteCount = 0;
    int appendCount = 0;
    int skipCount = 0;

    for (const SymbolData& symbol : symbols) {
        QString symbolName = symbol.info().name;

        if (existingSymbols.contains(symbolName)) {
            if (appendMode && !updateMode) {
                // 追加模式（非更新）：跳过已存在的符号
                qDebug() << "Symbol already exists, skipping (append mode):" << symbolName;
                skipCount++;
            } else {
                // 更新模式或覆盖模式：替换已存在的符号
                qDebug() << "Symbol already exists, overwriting (update mode):" << symbolName;
                overwriteCount++;
                symbolsToExport.append(symbol);
            }
        } else {
            // 新符�?
            qDebug() << "New symbol, adding:" << symbolName;
            appendCount++;
            symbolsToExport.append(symbol);
        }
    }

    qDebug() << "Symbols to export:" << symbolsToExport.count() << "(Overwrite:" << overwriteCount
             << ", Append:" << appendCount << ", Skip:" << skipCount << ")";

    // 如果没有符号需要导出，直接返回
    if (symbolsToExport.isEmpty()) {
        qDebug() << "No symbols to export, skipping";
        return true;
    }

    // 打开文件进行写入
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // 生成头部
    out << generateHeader(libName);

    // 生成所有符号（包括未覆盖的现有符号和新导出的符号）
    int index = 0;

    // 收集要被覆盖的符号名�?
    QSet<QString> overwrittenSymbolNames;
    for (const SymbolData& symbol : symbolsToExport) {
        overwrittenSymbolNames.insert(symbol.info().name);
    }

    // 收集要被删除的子符号名称（属于被覆盖的父符号的子符号�?
    // 改进的逻辑：准确识别分体式符号的子符号
    QSet<QString> subSymbolsToDelete;

    // 首先分析现有符号，确定哪些是分体式符号（有多个子符号�?
    QMap<QString, QStringList> parentToSubSymbols;  // 父符号名 -> 子符号列�?
    for (const QString& subSymbolName : subSymbolNames) {
        // 从子符号名称中提取父符号�?
        // 子符号格式：{parentName}_{unitNumber}_1
        int lastUnderscore = subSymbolName.lastIndexOf('_');
        if (lastUnderscore > 0) {
            int secondLastUnderscore = subSymbolName.lastIndexOf('_', lastUnderscore - 1);
            if (secondLastUnderscore > 0) {
                QString parentName = subSymbolName.left(secondLastUnderscore);
                parentToSubSymbols[parentName].append(subSymbolName);
            }
        }
    }

    // 对于每个被覆盖的父符号，收集其所有子符号
    for (const QString& parentSymbolName : overwrittenSymbolNames) {
        if (parentToSubSymbols.contains(parentSymbolName)) {
            // 这是一个分体式符号，删除所有子符号
            for (const QString& subSymbolName : parentToSubSymbols[parentSymbolName]) {
                subSymbolsToDelete.insert(subSymbolName);
                qDebug() << "Marking sub-symbol for deletion (multipart):" << subSymbolName
                         << "(parent:" << parentSymbolName << ")";
            }
        } else {
            // 这是一个单体符号，查找其子符号（格式：{parentName}_0_1�?
            QString expectedSubSymbolName = parentSymbolName + "_0_1";
            if (subSymbolNames.contains(expectedSubSymbolName)) {
                subSymbolsToDelete.insert(expectedSubSymbolName);
                qDebug() << "Marking sub-symbol for deletion (single part):" << expectedSubSymbolName
                         << "(parent:" << parentSymbolName << ")";
            }
        }
    }

    // 先导出未覆盖的现有符号（需要过滤掉被覆盖的子符号和以被覆盖符号名开头的顶层符号�?
    for (const QString& symbolName : existingSymbols.keys()) {
        bool isOverwritten = overwrittenSymbolNames.contains(symbolName);

        // 检查是否是以被覆盖符号名开头的顶层符号（可能是之前错误导出的子符号�?
        bool isOrphanedSubSymbol = false;
        if (!isOverwritten) {
            for (const QString& parentSymbolName : overwrittenSymbolNames) {
                if (symbolName.startsWith(parentSymbolName + "_")) {
                    isOrphanedSubSymbol = true;
                    qDebug() << "Skipping orphaned sub-symbol (top-level):" << symbolName
                             << "(parent:" << parentSymbolName << ")";
                    break;
                }
            }
        }

        if (!isOverwritten && !isOrphanedSubSymbol) {
            // 导出未覆盖的符号，但需要过滤掉子符�?
            QString symbolContent = existingSymbols[symbolName];
            QStringList lines = symbolContent.split('\n');
            QString filteredContent;
            bool skipNextSymbol = false;
            int braceCount = 0;

            for (const QString& line : lines) {
                QString trimmedLine = line.trimmed();

                // 检查是否是子符号定义的开�?
                if (trimmedLine.startsWith("(symbol \"")) {
                    int nameStart = trimmedLine.indexOf("\"") + 1;
                    int nameEnd = trimmedLine.indexOf("\"", nameStart);
                    if (nameEnd > nameStart) {
                        QString subSymbolName = trimmedLine.mid(nameStart, nameEnd - nameStart);
                        if (subSymbolsToDelete.contains(subSymbolName)) {
                            skipNextSymbol = true;
                            braceCount = 1;
                            qDebug() << "Skipping deleted sub-symbol:" << subSymbolName;
                            continue;
                        }
                    }
                }

                if (skipNextSymbol) {
                    // 计算括号数量
                    for (int j = 0; j < line.length(); ++j) {
                        if (line[j] == '(')
                            braceCount++;
                        else if (line[j] == ')') {
                            braceCount--;
                            if (braceCount == 0) {
                                skipNextSymbol = false;
                                break;
                            }
                        }
                    }
                    continue;
                }

                filteredContent += line + "\n";
            }

            out << filteredContent;
            qDebug() << "Keeping existing symbol:" << symbolName;
        }
    }

    // 再导出新符号和被覆盖的符�?
    for (const SymbolData& symbol : symbolsToExport) {
        qDebug() << "Exporting symbol" << (++index) << "of" << symbolsToExport.count() << ":" << symbol.info().name;
        out << generateSymbolContent(symbol, libName);
    }

    // 生成尾部
    out << ")\n";  // 闭合 kicad_symbol_lib

    file.close();
    qDebug() << "Symbol library exported successfully:" << filePath;
    return true;
}
QString ExporterSymbol::generateHeader(const QString& libName) const {
    Q_UNUSED(libName);
    return QString(
        "(kicad_symbol_lib\n"
        "  (version 20211014)\n"
        "  (generator https://github.com/tangsangsimida/EasyKiConverter)\n");
}

QString ExporterSymbol::generateSymbolContent(const SymbolData& symbolData, const QString& libName) const {
    QString content;

    // V6 格式 - 主符号定义（包含属性）
    // 在更新模式下，保持符号名称不变；在新符号中，替换空格为下划线
    QString cleanSymbolName = symbolData.info().name;
    // 注意：KiCad 6.x 允许符号名称中包含空格，所以不需要替�?
    // 保持原始名称以确保更新模式下能正确匹配和替换
    content += QString("  (symbol \"%1\"\n").arg(cleanSymbolName);
    content += "    (in_bom yes)\n";
    content += "    (on_board yes)\n";

    // 设置当前边界框，用于图形元素的相对坐标计�?
    SymbolBBox originalBBox = symbolData.bbox();
    m_currentBBox = originalBBox;
    qDebug() << "BBox - x:" << m_currentBBox.x << "y:" << m_currentBBox.y << "width:" << m_currentBBox.width
             << "height:" << m_currentBBox.height;
    // 计算符号中心点（用于将符号居中显示）
    double centerX = m_currentBBox.x + m_currentBBox.width / 2.0;
    double centerY = m_currentBBox.y + m_currentBBox.height / 2.0;
    qDebug() << "Symbol center - centerX:" << centerX << "centerY:" << centerY;
    // 修改边界框，使其指向中心点，这样所有图形元素都会相对于中心点定�?
    m_currentBBox.x = centerX;
    m_currentBBox.y = centerY;
    qDebug() << "Adjusted BBox for centering - x:" << m_currentBBox.x << "y:" << m_currentBBox.y;
    // 计算 y_high �?y_low（使用引脚坐标，�?Python 版本保持一致）
    // 如果没有引脚，使用默认值以确保属性位置正�?
    double yHigh = 2.54;  // 默认值：100mil
    double yLow = -2.54;  // 默认值：-100mil
    QList<SymbolPin> pins = symbolData.pins();
    if (!pins.isEmpty()) {
        qDebug() << "Pin coordinates calculation:";
        for (const SymbolPin& pin : pins) {
            double pinY = -pxToMm(pin.settings.posY - centerY);
            qDebug() << "  Pin" << pin.settings.spicePinNumber << "- raw Y:" << pin.settings.posY
                     << "converted Y:" << pinY;
            yHigh = qMax(yHigh, pinY);
            yLow = qMin(yLow, pinY);
        }
    } else {
        qDebug() << "No pins found, using default yHigh and yLow values";
    }
    qDebug() << "Final yHigh:" << yHigh << "yLow:" << yLow;

    // 生成属�?
    double fieldOffset = 5.08;  // FIELD_OFFSET_START
    double fontSize = 1.27;     // PROPERTY_FONT_SIZE

    // 辅助函数：转义属性�?
    auto escapePropertyValue = [](const QString& value) -> QString {
        QString escaped = value;
        escaped.replace("\"", "\\\"");  // 转义引号
        escaped.replace("\n", " ");     // 移除换行�?
        escaped.replace("\t", " ");     // 移除制表�?
        return escaped.trimmed();
    };

    // Reference 属�?
    QString refPrefix = symbolData.info().prefix;
    refPrefix.replace("?", "");  // 移除 "?" 后缀
    content += QString("    (property\n");
    content += QString("      \"Reference\"\n");
    content += QString("      \"%1\"\n").arg(escapePropertyValue(refPrefix));
    content += "      (id 0)\n";
    content += QString("      (at 0 %1 0)\n").arg(yHigh + fieldOffset, 0, 'f', 2);
    content += QString("      (effects (font (size %1 %2) (thickness 0) ) )\n")
                   .arg(fontSize, 0, 'f', 2)
                   .arg(fontSize, 0, 'f', 2);
    content += "    )\n";

    // Value 属�?
    content += QString("    (property\n");
    content += QString("      \"Value\"\n");
    content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().name));
    content += "      (id 1)\n";
    content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
    content += QString("      (effects (font (size %1 %2) (thickness 0) ) )\n")
                   .arg(fontSize, 0, 'f', 2)
                   .arg(fontSize, 0, 'f', 2);
    content += "    )\n";

    // Footprint 属�?
    if (!symbolData.info().package.isEmpty()) {
        fieldOffset += 2.54;  // FIELD_OFFSET_INCREMENT
        content += QString("    (property\n");
        content += QString("      \"Footprint\"\n");
        // 添加库前缀：libName:package
        QString footprintPath = QString("%1:%2").arg(libName, symbolData.info().package);
        content += QString("      \"%1\"\n").arg(escapePropertyValue(footprintPath));
        content += "      (id 2)\n";
        content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
        content += QString("      (effects (font (size %1 %2) (thickness 0) ) hide)\n")
                       .arg(fontSize, 0, 'f', 2)
                       .arg(fontSize, 0, 'f', 2);
        content += "    )\n";
    }

    // Datasheet 属�?
    if (!symbolData.info().datasheet.isEmpty()) {
        fieldOffset += 2.54;
        content += QString("    (property\n");
        content += QString("      \"Datasheet\"\n");
        content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().datasheet));
        content += "      (id 3)\n";
        content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
        content += QString("      (effects (font (size %1 %2) (thickness 0) ) hide)\n")
                       .arg(fontSize, 0, 'f', 2)
                       .arg(fontSize, 0, 'f', 2);
        content += "    )\n";
    }

    // Manufacturer 属�?
    if (!symbolData.info().manufacturer.isEmpty()) {
        fieldOffset += 2.54;
        content += QString("    (property\n");
        content += QString("      \"Manufacturer\"\n");
        content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().manufacturer));
        content += "      (id 4)\n";
        content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
        content += QString("      (effects (font (size %1 %2) (thickness 0) ) hide)\n")
                       .arg(fontSize, 0, 'f', 2)
                       .arg(fontSize, 0, 'f', 2);
        content += "    )\n";
    }

    // LCSC Part 属�?
    if (!symbolData.info().lcscId.isEmpty()) {
        fieldOffset += 2.54;
        content += QString("    (property\n");
        content += QString("      \"LCSC Part\"\n");
        content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().lcscId));
        content += "      (id 5)\n";
        content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
        content += QString("      (effects (font (size %1 %2) (thickness 0) ) hide)\n")
                       .arg(fontSize, 0, 'f', 2)
                       .arg(fontSize, 0, 'f', 2);
        content += "    )\n";
    }

    // 检查是否为多部分符�?
    bool isMultiPart = symbolData.isMultiPart();
    qDebug() << "=== Symbol Type Check ===";
    qDebug() << "Symbol name:" << cleanSymbolName;
    qDebug() << "Parts count:" << symbolData.parts().size();
    qDebug() << "Is multi-part:" << isMultiPart;

    if (isMultiPart) {
        qDebug() << "Exporting multi-part symbol with" << symbolData.parts().size() << "parts";

        // 为每个部分生成子符号
        for (const SymbolPart& part : symbolData.parts()) {
            qDebug() << "Generating sub-symbol" << part.unitNumber << "with" << part.pins.size() << "pins";
            content += generateSubSymbol(symbolData, part, cleanSymbolName, libName, centerX, centerY);
        }
    } else {
        // 单部分符号：直接在主符号中包含图形元素，不使用子符号
        qDebug() << "Exporting single-part symbol";

        // 生成图形元素
        for (const SymbolRectangle& rect : symbolData.rectangles()) {
            content += generateRectangle(rect);
        }
        for (const SymbolCircle& circle : symbolData.circles()) {
            content += generateCircle(circle);
        }
        for (const SymbolArc& arc : symbolData.arcs()) {
            content += generateArc(arc);
        }
        for (const SymbolEllipse& ellipse : symbolData.ellipses()) {
            content += generateEllipse(ellipse);
        }
        for (const SymbolPolygon& polygon : symbolData.polygons()) {
            content += generatePolygon(polygon);
        }
        for (const SymbolPolyline& polyline : symbolData.polylines()) {
            content += generatePolyline(polyline);
        }
        for (const SymbolPath& path : symbolData.paths()) {
            content += generatePath(path);
        }

        // 生成文本元素
        for (const SymbolText& text : symbolData.texts()) {
            content += generateText(text);
        }

        // 生成引脚
        for (const SymbolPin& pin : symbolData.pins()) {
            content += generatePin(pin, m_currentBBox);
        }
    }

    content += "  )\n";  // 结束主符�?

    qDebug() << "=== Generated Symbol Content ===";
    qDebug() << "Content length:" << content.length();
    qDebug() << "Content contains sub-symbol 0:" << content.contains("MCIMX6Y2CVM08AB_0_1");
    qDebug() << "Content contains sub-symbol 1:" << content.contains("MCIMX6Y2CVM08AB_1_1");
    qDebug() << "Content preview (first 1000 chars):" << content.left(1000);
    qDebug() << "Content preview (last 1000 chars):" << content.right(1000);

    return content;
}

QString ExporterSymbol::generateSubSymbol(const SymbolData& symbolData,
                                          const QString& symbolName,
                                          const QString& libName,
                                          double centerX,
                                          double centerY) const {
    QString content;
    // 单部分符号：使用 _0_1 作为子符号名�?
    content += QString("    (symbol \"%1_0_1\"\n").arg(symbolName);
    // 生成图形元素（直接生成，不添加任何属性）
    for (const SymbolRectangle& rect : symbolData.rectangles()) {
        content += generateRectangle(rect);
    }
    for (const SymbolCircle& circle : symbolData.circles()) {
        content += generateCircle(circle);
    }
    for (const SymbolArc& arc : symbolData.arcs()) {
        content += generateArc(arc);
    }
    for (const SymbolEllipse& ellipse : symbolData.ellipses()) {
        content += generateEllipse(ellipse);
    }
    for (const SymbolPolygon& polygon : symbolData.polygons()) {
        content += generatePolygon(polygon);
    }
    for (const SymbolPolyline& polyline : symbolData.polylines()) {
        content += generatePolyline(polyline);
    }
    for (const SymbolPath& path : symbolData.paths()) {
        content += generatePath(path);
    }
    // 生成文本元素
    for (const SymbolText& text : symbolData.texts()) {
        content += generateText(text);
    }
    // 生成引脚（使用中心点计算坐标，让引脚也跟着图形一起移动）
    SymbolBBox centeredBBox;
    centeredBBox.x = centerX;
    centeredBBox.y = centerY;
    centeredBBox.width = symbolData.bbox().width;
    centeredBBox.height = symbolData.bbox().height;
    for (const SymbolPin& pin : symbolData.pins()) {
        content += generatePin(pin, centeredBBox);
    }
    content += "    )\n";  // 结束子符�?
    return content;
}

QString ExporterSymbol::generateSubSymbol(const SymbolData& symbolData,
                                          const SymbolPart& part,
                                          const QString& symbolName,
                                          const QString& libName,
                                          double centerX,
                                          double centerY) const {
    QString content;

    // 多部分符号：使用 _{unitNumber}_1 作为子符号名?
    // 注意：Unit 编号必须?1 开始，而不是从 0 开?
    content += QString("    (symbol \"%1_%2_1\"\n").arg(symbolName).arg(part.unitNumber + 1);

    // 计算子部分的边界框和中心点
    SymbolBBox partBBox = calculatePartBBox(part);
    double partCenterX = partBBox.x + partBBox.width / 2.0;
    double partCenterY = partBBox.y + partBBox.height / 2.0;

    qDebug() << "Sub-symbol" << part.unitNumber << "- partCenterX:" << partCenterX << "partCenterY:" << partCenterY;

    // 临时保存当前的边界框
    SymbolBBox originalBBox = m_currentBBox;

    // 设置 m_currentBBox 为子部分的中心点，以便图形元素使用正确的偏移
    m_currentBBox.x = partCenterX;
    m_currentBBox.y = partCenterY;
    m_currentBBox.width = partBBox.width;
    m_currentBBox.height = partBBox.height;

    qDebug() << "Setting m_currentBBox for sub-symbol - x:" << m_currentBBox.x << "y:" << m_currentBBox.y;

    // 创建以子部分中心为基准的边界框（用于引脚）
    SymbolBBox centeredPartBBox;
    centeredPartBBox.x = partCenterX;
    centeredPartBBox.y = partCenterY;
    centeredPartBBox.width = partBBox.width;
    centeredPartBBox.height = partBBox.height;

    // 生成图形元素（直接生成，不添加任何属性）
    for (const SymbolRectangle& rect : part.rectangles) {
        content += generateRectangle(rect);
    }
    for (const SymbolCircle& circle : part.circles) {
        content += generateCircle(circle);
    }
    for (const SymbolArc& arc : part.arcs) {
        content += generateArc(arc);
    }
    for (const SymbolEllipse& ellipse : part.ellipses) {
        content += generateEllipse(ellipse);
    }
    for (const SymbolPolygon& polygon : part.polygons) {
        content += generatePolygon(polygon);
    }
    for (const SymbolPolyline& polyline : part.polylines) {
        content += generatePolyline(polyline);
    }
    for (const SymbolPath& path : part.paths) {
        content += generatePath(path);
    }

    // 生成文本元素
    for (const SymbolText& text : part.texts) {
        content += generateText(text);
    }

    // 生成引脚（使用子部分的中心点进行偏移）
    for (const SymbolPin& pin : part.pins) {
        content += generatePin(pin, centeredPartBBox);
    }

    // 恢复原始边界框
    m_currentBBox = originalBBox;

    content += "    )\n";  // 结束子符?

    return content;
}

QString ExporterSymbol::generatePin(const SymbolPin& pin, const SymbolBBox& bbox) const {
    QString content;

    // 使用边界框偏移量计算相对坐标
    double x = pxToMm(pin.settings.posX - bbox.x);
    double y = -pxToMm(pin.settings.posY - bbox.y);  // Y 轴翻�?

    // 计算引脚长度（Python版本的做法：直接从路径字符串中提�?h'后面的数字）
    QString path = pin.pinPath.path;
    double length = 0;

    // 查找 'h' 命令并提取其后的数值作为引脚长�?
    int hIndex = path.indexOf('h');
    if (hIndex >= 0) {
        QString lengthStr = path.mid(hIndex + 1);
        // 提取数值部分（可能包含其他命令�?
        QStringList parts = lengthStr.split(QRegularExpression("[^0-9.-]"), Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            length = parts[0].toDouble();
        }
    }

    // 转换为毫米单�?
    length = pxToMm(length);

    // 调试：输出引脚信�?
    qDebug() << "Pin Debug - Name:" << pin.name.text << "Number:" << pin.settings.spicePinNumber;
    qDebug() << "  Original posX:" << pin.settings.posX << "bbox.x:" << bbox.x;
    qDebug() << "  Calculated x:" << x << "y:" << y;
    qDebug() << "  Path:" << path << "Extracted length (px):" << length / 0.0254;
    qDebug() << "  Length (mm):" << length << "Rotation:" << pin.settings.rotation;

    // 确保引脚长度为正数（KiCad 引脚长度必须是正数）
    length = std::abs(length);

    // 确保引脚长度不为 0
    if (length < 0.01) {
        length = 2.54;  // 默认引脚长度�?00mil�?
    }

    // 直接使用原始引脚类型，不进行自动推断
    PinType pinType = pin.settings.type;
    QString kicadPinType = pinTypeToKicad(pinType);

    // 动态计算引脚样式（根据 dot �?clock 的显示状态）
    PinStyle pinStyle = PinStyle::Line;
    if (pin.dot.isDisplayed && pin.clock.isDisplayed) {
        pinStyle = PinStyle::InvertedClock;
    } else if (pin.dot.isDisplayed) {
        pinStyle = PinStyle::Inverted;
    } else if (pin.clock.isDisplayed) {
        pinStyle = PinStyle::Clock;
    }
    QString kicadPinStyle = pinStyleToKicad(pinStyle);

    // 处理引脚名称和编�?
    QString pinName = pin.name.text;
    pinName.replace(" ", "");
    QString pinNumber = pin.settings.spicePinNumber;
    pinNumber.replace(" ", "");

    // 如果引脚名称为空，使用引脚编�?
    if (pinName.isEmpty()) {
        pinName = pinNumber;
    }

    double orientation = pin.settings.rotation;

    // Python 版本使用 (180 + orientation) % 360 计算方向
    // 注意：orientation �?0, 90, 180, 270 的整�?
    double kicadOrientation = (180.0 + orientation);
    while (kicadOrientation >= 360.0)
        kicadOrientation -= 360.0;

    // 规范化为 KiCad 要求的标准角度：0, 90, 180, 270
    // 找到最接近的标准角�?
    double standardAngles[] = {0.0, 90.0, 180.0, 270.0};
    double closestAngle = 0.0;
    double minDiff = 360.0;
    for (double angle : standardAngles) {
        double diff = qAbs(kicadOrientation - angle);
        if (diff < minDiff) {
            minDiff = diff;
            closestAngle = angle;
        }
    }
    kicadOrientation = closestAngle;

    content += QString("    (pin %1 %2\n").arg(kicadPinType, kicadPinStyle);
    content += QString("      (at %1 %2 %3)\n").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(kicadOrientation, 0, 'f', 0);
    content += QString("      (length %1)\n").arg(length, 0, 'f', 2);
    content += QString("      (name \"%1\" (effects (font (size 1.27 1.27) (thickness 0) )))\n").arg(pinName);
    content += QString("      (number \"%1\" (effects (font (size 1.27 1.27) (thickness 0) )))\n").arg(pinNumber);
    content += "    )\n";

    return content;
}

QString ExporterSymbol::generateRectangle(const SymbolRectangle& rect) const {
    QString content;

    // V6 使用毫米单位
    // 使用原始矩形的坐标和尺寸
    double x0 = pxToMm(rect.posX - m_currentBBox.x);
    double y0 = -pxToMm(rect.posY - m_currentBBox.y);  // Y 轴翻�?
    double x1 = pxToMm(rect.posX + rect.width - m_currentBBox.x);
    double y1 = -pxToMm(rect.posY + rect.height - m_currentBBox.y);  // Y 轴翻�?
    double strokeWidth = pxToMm(rect.strokeWidth);

    content += "    (rectangle\n";
    content += QString("      (start %1 %2)\n").arg(x0, 0, 'f', 2).arg(y0, 0, 'f', 2);
    content += QString("      (end %1 %2)\n").arg(x1, 0, 'f', 2).arg(y1, 0, 'f', 2);
    content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
    content += "      (fill (type none))\n";
    content += "    )\n";

    return content;
}

QString ExporterSymbol::generateCircle(const SymbolCircle& circle) const {
    QString content;

    // V6 使用毫米单位
    double cx = pxToMm(circle.centerX - m_currentBBox.x);
    double cy = -pxToMm(circle.centerY - m_currentBBox.y);  // Y 轴翻�?
    double radius = pxToMm(circle.radius);
    double strokeWidth = pxToMm(circle.strokeWidth);

    content += "    (circle\n";
    content += QString("      (center %1 %2)\n").arg(cx, 0, 'f', 2).arg(cy, 0, 'f', 2);
    content += QString("      (radius %1)\n").arg(radius, 0, 'f', 2);
    content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
    content += "      (fill (type none))\n";
    content += "    )\n";

    return content;
}

QString ExporterSymbol::generateArc(const SymbolArc& arc) const {
    QString content;
    double strokeWidth = pxToMm(arc.strokeWidth);

    // KiCad V6 使用三点法定义圆弧：start、mid、end
    if (arc.path.size() >= 3) {
        // 提取起点、中点、终�?
        const auto path = arc.path;  // 避免临时对象detach
        QPointF startPoint = path.first();
        QPointF endPoint = path.last();

        // 计算中点（取中间的点�?
        int midIndex = path.size() / 2;
        QPointF midPoint = path[midIndex];

        // 转换为相对于边界框的坐标，并转换为毫�?
        double startX = pxToMm(startPoint.x() - m_currentBBox.x);
        double startY = -pxToMm(startPoint.y() - m_currentBBox.y);  // Y 轴翻�?
        double midX = pxToMm(midPoint.x() - m_currentBBox.x);
        double midY = -pxToMm(midPoint.y() - m_currentBBox.y);  // Y 轴翻�?
        double endX = pxToMm(endPoint.x() - m_currentBBox.x);
        double endY = -pxToMm(endPoint.y() - m_currentBBox.y);  // Y 轴翻�?

        content += "    (arc\n";
        content += QString("      (start %1 %2)\n").arg(startX, 0, 'f', 2).arg(startY, 0, 'f', 2);
        content += QString("      (mid %1 %2)\n").arg(midX, 0, 'f', 2).arg(midY, 0, 'f', 2);
        content += QString("      (end %1 %2)\n").arg(endX, 0, 'f', 2).arg(endY, 0, 'f', 2);
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);

        // 根据fillColor设置填充类型（与Python版本一致）
        if (arc.fillColor) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }

        content += "    )\n";
    } else if (arc.path.size() == 2) {
        // 只有两个点，计算中点
        const auto path = arc.path;  // 避免临时对象detach
        QPointF startPoint = path.first();
        QPointF endPoint = path.last();
        QPointF midPoint = (startPoint + endPoint) / 2;

        // 转换为相对于边界框的坐标，并转换为毫�?
        double startX = pxToMm(startPoint.x() - m_currentBBox.x);
        double startY = -pxToMm(startPoint.y() - m_currentBBox.y);  // Y 轴翻�?
        double midX = pxToMm(midPoint.x() - m_currentBBox.x);
        double midY = -pxToMm(midPoint.y() - m_currentBBox.y);  // Y 轴翻�?
        double endX = pxToMm(endPoint.x() - m_currentBBox.x);
        double endY = -pxToMm(endPoint.y() - m_currentBBox.y);  // Y 轴翻�?

        content += "    (arc\n";
        content += QString("      (start %1 %2)\n").arg(startX, 0, 'f', 2).arg(startY, 0, 'f', 2);
        content += QString("      (mid %1 %2)\n").arg(midX, 0, 'f', 2).arg(midY, 0, 'f', 2);
        content += QString("      (end %1 %2)\n").arg(endX, 0, 'f', 2).arg(endY, 0, 'f', 2);
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);

        // 根据fillColor设置填充类型
        if (arc.fillColor) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }

        content += "    )\n";
    } else {
        // 点数不足，跳过或生成一个默认的arc
        qDebug() << "Warning: Arc has insufficient points (" << arc.path.size() << "), skipping";
    }

    return content;
}

QString ExporterSymbol::generateEllipse(const SymbolEllipse& ellipse) const {
    QString content;

    // V6 使用毫米单位
    double cx = pxToMm(ellipse.centerX - m_currentBBox.x);
    double cy = -pxToMm(ellipse.centerY - m_currentBBox.y);  // Y 轴翻�?
    double radiusX = pxToMm(ellipse.radiusX);
    double radiusY = pxToMm(ellipse.radiusY);
    double strokeWidth = pxToMm(ellipse.strokeWidth);

    // 如果是圆形（radiusX �?radiusY），使用 circle 元素
    if (qAbs(radiusX - radiusY) < 0.01) {
        content += "    (circle\n";
        content += QString("      (center %1 %2)\n").arg(cx, 0, 'f', 2).arg(cy, 0, 'f', 2);
        content += QString("      (radius %1)\n").arg(radiusX, 0, 'f', 2);
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
        content += "      (fill (type none))\n";
        content += "    )\n";
    } else {
        // 椭圆：转换为路径
        // 使用 32 段折线近似椭�?
        content += "    (polyline\n";
        content += "      (pts";

        const int segments = 32;
        for (int i = 0; i <= segments; ++i) {
            double angle = 2.0 * M_PI * i / segments;
            double x = cx + radiusX * std::cos(angle);
            double y = cy + radiusY * std::sin(angle);
            content += QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);
        }

        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);

        // 根据 fillColor 属性设置填充类�?
        if (ellipse.fillColor) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }
        content += "    )\n";
    }

    return content;
}

QString ExporterSymbol::generatePolygon(const SymbolPolygon& polygon) const {
    QString content;
    double strokeWidth = pxToMm(polygon.strokeWidth);

    // 解析点数�?
    QStringList points = polygon.points.split(" ");
    // 过滤掉空字符�?
    points.removeAll("");

    // 至少需�?2 个有效的点（4 个坐标值）
    if (points.size() >= 4) {
        // KiCad V6 不支�?polygon 元素，使�?polyline 代替
        content += "    (polyline\n";
        content += "      (pts";
        // 存储第一个点以便在最后重�?
        QString firstPoint;
        QString lastPoint;  // 用于检测重复点
        for (int i = 0; i < points.size(); i += 2) {
            if (i + 1 < points.size()) {
                // 转换为相对于边界框的坐标，并转换为毫�?
                double x = pxToMm(points[i].toDouble() - m_currentBBox.x);
                double y = -pxToMm(points[i + 1].toDouble() - m_currentBBox.y);
                QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);

                // 避免重复�?
                if (point != lastPoint) {
                    content += point;
                    if (i == 0) {
                        firstPoint = point;
                    }
                    lastPoint = point;
                }
            }
        }
        // 多边形总是重复第一个点以闭�?
        if (!firstPoint.isEmpty() && firstPoint != lastPoint) {
            content += firstPoint;
        }
        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
        // 根据 fillColor 属性设置填充类�?
        if (polygon.fillColor) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }
        content += "    )\n";
    }

    return content;
}
QString ExporterSymbol::generatePolyline(const SymbolPolyline& polyline) const {
    QString content;
    double strokeWidth = pxToMm(polyline.strokeWidth);

    // 解析点数�?
    QStringList points = polyline.points.split(" ");
    // 过滤掉空字符�?
    points.removeAll("");

    // 至少需�?2 个有效的点（4 个坐标值）
    if (points.size() >= 4) {
        content += "    (polyline\n";
        content += "      (pts";
        // 存储第一个点
        QString firstPoint;
        QString lastPoint;  // 用于检测重复点
        for (int i = 0; i < points.size(); i += 2) {
            if (i + 1 < points.size()) {
                // 转换为相对于边界框的坐标，并转换为毫�?
                double x = pxToMm(points[i].toDouble() - m_currentBBox.x);
                double y = -pxToMm(points[i + 1].toDouble() - m_currentBBox.y);
                QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);

                // 避免重复�?
                if (point != lastPoint) {
                    content += point;
                    if (i == 0) {
                        firstPoint = point;
                    }
                    lastPoint = point;
                }
            }
        }
        // 只有�?fillColor �?true 时才重复第一个点
        if (polyline.fillColor && !firstPoint.isEmpty() && firstPoint != lastPoint) {
            content += firstPoint;
        }
        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(strokeWidth, 0, 'f', 3);
        // 填充类型�?fillColor 决定
        if (polyline.fillColor) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }
        content += "    )\n";
    }

    return content;
}
QString ExporterSymbol::generatePath(const SymbolPath& path) const {
    QString content;

    // 使用SvgPathParser解析SVG路径
    QList<QPointF> points = SvgPathParser::parsePath(path.paths);

    // 生成polyline
    if (!points.isEmpty()) {
        content += "    (polyline\n";
        content += "      (pts";

        QString lastPoint;
        for (const QPointF& pt : points) {
            // 转换为相对于边界框的坐标，并转换为毫�?
            double x = pxToMm(pt.x() - m_currentBBox.x);
            double y = -pxToMm(pt.y() - m_currentBBox.y);  // Y 轴翻�?

            QString point = QString(" (xy %1 %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);

            // 避免重复�?
            if (point != lastPoint) {
                content += point;
                lastPoint = point;
            }
        }

        content += ")\n";
        content += QString("      (stroke (width %1) (type default))\n").arg(pxToMm(path.strokeWidth), 0, 'f', 3);

        // 填充类型�?fillColor 决定
        if (path.fillColor) {
            content += "      (fill (type background))\n";
        } else {
            content += "      (fill (type none))\n";
        }

        content += "    )\n";
    } else {
        // 如果没有有效点，生成占位�?
        content += "    (polyline (pts (xy 0 0))\n";
        content += "      (stroke (width 0.127) (type default))\n";
        content += "      (fill (type none))\n";
        content += "    )\n";
    }

    return content;
}

QString ExporterSymbol::generateText(const SymbolText& text) const {
    QString content;

    // V6 使用毫米单位
    double x = pxToMm(text.posX - m_currentBBox.x);
    double y = -pxToMm(text.posY - m_currentBBox.y);  // Y 轴翻�?

    // 计算字体大小（从pt转换为mm�?
    double fontSize = text.textSize * 0.352778;  // 1pt = 0.352778mm

    // 处理粗体和斜�?
    QString fontStyle = "";
    if (text.bold) {
        fontStyle += "bold ";
    }
    if (text.italic == "1" || text.italic == "Italic" || text.italic == "italic") {
        fontStyle += "italic ";
    }
    if (fontStyle.isEmpty()) {
        fontStyle = "normal";
    } else {
        fontStyle = fontStyle.trimmed();
    }

    // 处理旋转角度
    double rotation = text.rotation;
    if (rotation != 0) {
        rotation = 360 - rotation;
    }

    // 处理可见�?
    QString hide = text.visible ? "" : "hide";

    // 转义文本内容
    QString textContent = text.text;
    textContent.replace(" ", "");
    if (textContent.isEmpty()) {
        textContent = "~";
    }

    // 生成文本元素
    content += "    (text\n";
    content += QString("      \"%1\"\n").arg(textContent);
    content += QString("      (at %1 %2 %3)\n").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(rotation, 0, 'f', 0);
    content += QString("      (effects (font (size %1 %2) (thickness 0.1) %3) %4)\n")
                   .arg(fontSize, 0, 'f', 2)
                   .arg(fontSize, 0, 'f', 2)
                   .arg(fontStyle)
                   .arg(hide);
    content += "    )\n";

    return content;
}

double ExporterSymbol::pxToMil(double px) const {
    return 10.0 * px;
}

double ExporterSymbol::pxToMm(double px) const {
    return GeometryUtils::convertToMm(px);
}

QString ExporterSymbol::pinTypeToKicad(PinType pinType) const {
    switch (pinType) {
        case PinType::Unspecified:
            return "unspecified";
        case PinType::Input:
            return "input";
        case PinType::Output:
            return "output";
        case PinType::Bidirectional:
            return "bidirectional";
        case PinType::Power:
            return "power_in";
        default:
            return "unspecified";
    }
}

QString ExporterSymbol::pinStyleToKicad(PinStyle pinStyle) const {
    switch (pinStyle) {
        case PinStyle::Line:
            return "line";
        case PinStyle::Inverted:
            return "inverted";
        case PinStyle::Clock:
            return "clock";
        case PinStyle::InvertedClock:
            return "inverted_clock";
        case PinStyle::InputLow:
            return "input_low";
        case PinStyle::ClockLow:
            return "clock_low";
        case PinStyle::OutputLow:
            return "output_low";
        case PinStyle::EdgeClockHigh:
            return "edge_clock_high";
        case PinStyle::NonLogic:
            return "non_logic";
        default:
            return "line";
    }
}

QString ExporterSymbol::rotationToKicadOrientation(int rotation) const {
    switch (rotation) {
        case 0:
            return "right";
        case 90:
            return "up";
        case 180:
            return "left";
        case 270:
            return "down";
        default:
            return "right";
    }
}

SymbolBBox ExporterSymbol::calculatePartBBox(const SymbolPart& part) const {
    SymbolBBox bbox;
    bbox.x = 0.0;
    bbox.y = 0.0;
    bbox.width = 0.0;
    bbox.height = 0.0;

    // 如果子部分有坐标原点，使用它作为基准
    if (part.originX != 0.0 || part.originY != 0.0) {
        bbox.x = part.originX;
        bbox.y = part.originY;
    }

    // 计算图形元素的边界
    double minX = bbox.x;
    double minY = bbox.y;
    double maxX = bbox.x;
    double maxY = bbox.y;

    // 遍历所有图形元素，计算边界
    auto updateBounds = [&](double x, double y, double w, double h) {
        minX = qMin(minX, x);
        minY = qMin(minY, y);
        maxX = qMax(maxX, x + w);
        maxY = qMax(maxY, y + h);
    };

    // 处理矩形
    for (const auto& rect : part.rectangles) {
        updateBounds(rect.posX, rect.posY, rect.width, rect.height);
    }

    // 处理圆
    for (const auto& circle : part.circles) {
        double radius = circle.radius;
        updateBounds(circle.centerX - radius, circle.centerY - radius, radius * 2, radius * 2);
    }

    // 处理椭圆
    for (const auto& ellipse : part.ellipses) {
        updateBounds(ellipse.centerX - ellipse.radiusX,
                     ellipse.centerY - ellipse.radiusY,
                     ellipse.radiusX * 2,
                     ellipse.radiusY * 2);
    }

    // 处理圆弧（使用路径点）
    for (const auto& arc : part.arcs) {
        for (const auto& point : arc.path) {
            minX = qMin(minX, point.x());
            minY = qMin(minY, point.y());
            maxX = qMax(maxX, point.x());
            maxY = qMax(maxY, point.y());
        }
    }

    // 处理引脚
    for (const auto& pin : part.pins) {
        minX = qMin(minX, pin.settings.posX);
        minY = qMin(minY, pin.settings.posY);
        maxX = qMax(maxX, pin.settings.posX);
        maxY = qMax(maxY, pin.settings.posY);
    }

    // 处理文本
    for (const auto& text : part.texts) {
        minX = qMin(minX, text.posX);
        minY = qMin(minY, text.posY);
        maxX = qMax(maxX, text.posX);
        maxY = qMax(maxY, text.posY);
    }

    // 更新边界框
    bbox.x = minX;
    bbox.y = minY;
    bbox.width = maxX - minX;
    bbox.height = maxY - minY;

    qDebug() << "Part BBox - x:" << bbox.x << "y:" << bbox.y << "width:" << bbox.width << "height:" << bbox.height;

    return bbox;
}

}  // namespace EasyKiConverter
