#include "ExporterSymbol.h"

#include "KiCadExportMetadata.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include <cmath>
#include <limits>

namespace EasyKiConverter {

ExporterSymbol::ExporterSymbol() {}

ExporterSymbol::~ExporterSymbol() {}

bool ExporterSymbol::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // 生成符号内容（不包含库头，仅单个符号定义）
    QString content = generateSymbolContent(symbol, "");

    out << content;
    file.close();

    qDebug() << "Symbol exported to:" << filePath;
    return true;
}

bool ExporterSymbol::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                         const QString& libName,
                                         const QString& filePath,
                                         bool appendMode,
                                         bool updateMode,
                                         const QString& libraryDescription) {
    qDebug() << "=== Export Symbol Library ===";
    qDebug() << "Library name:" << libName;
    qDebug() << "Output path:" << filePath;
    qDebug() << "Symbol count:" << symbols.count();
    qDebug() << "KiCad version: V6";
    qDebug() << "Append mode:" << appendMode;
    qDebug() << "Update mode:" << updateMode;
    qDebug() << "Library description:" << libraryDescription;

    // 重置检测到的版本，避免影响后续调用
    m_detectedVersion.clear();

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

        // 生成所有符号
        int index = 0;
        for (const IR::SymbolComponentIR& symbol : symbols) {
            qDebug() << "Exporting symbol" << (++index) << "of" << symbols.count() << ":" << symbol.name;
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

    // 读取现有库内容
    QString existingContent;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        existingContent = QTextStream(&file).readAll();
        file.close();
        qDebug() << "Read" << existingContent.length() << "bytes from existing library";
    } else {
        qWarning() << "Failed to open existing library for reading:" << filePath;
        return false;
    }

    // 检测现有文件的版本
    QRegularExpression versionRegex(R"(^\s*\(version\s+(\d+)\))", QRegularExpression::MultilineOption);
    QRegularExpressionMatch versionMatch = versionRegex.match(existingContent);
    if (versionMatch.hasMatch()) {
        m_detectedVersion = versionMatch.captured(1);
        qDebug() << "Detected existing library version:" << m_detectedVersion;
    } else {
        m_detectedVersion = "20211014";
        qDebug() << "Could not detect version, using default:" << m_detectedVersion;
    }

    // 提取现有符号
    QMap<QString, QString> existingSymbols;  // 符号名-> 符号内容
    QSet<QString> subSymbolNames;  // 属于分体式符号的子符号名称

    // 使用栈来正确追踪符号的嵌套关系
    QStringList lines = existingContent.split('\n');
    int braceCount = 0;

    // 栈元素: (symbolName, startLine, braceCountAfterStart)
    struct SymbolInfo {
        QString name;
        int startLine;
        int braceAfterStart;
    };

    QList<SymbolInfo> symbolStack;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];

        // 逐字符计算括号变化
        for (int j = 0; j < line.length(); ++j) {
            if (line[j] == '(') {
                braceCount++;
            } else if (line[j] == ')') {
                braceCount--;
            }
        }

        // 检查是否有符号开始
        int symbolPosInLine = line.indexOf("(symbol \"");
        if (symbolPosInLine >= 0) {
            int nameStart = line.indexOf("\"", symbolPosInLine) + 1;
            int nameEnd = line.indexOf("\"", nameStart);
            if (nameEnd > nameStart) {
                QString symbolName = line.mid(nameStart, nameEnd - nameStart);
                SymbolInfo info;
                info.name = symbolName;
                info.startLine = i;
                info.braceAfterStart = braceCount;
                symbolStack.append(info);
                qDebug() << "Found symbol start:" << symbolName << "at line" << i << "with braceCount" << braceCount;
            }
        }

        // 检查是否有符号结束 - 当 braceCount 回到符号开始后的值时
        while (!symbolStack.isEmpty() && braceCount < symbolStack.last().braceAfterStart) {
            SymbolInfo info = symbolStack.last();
            symbolStack.removeLast();

            // 检查是否是子符号 - 如果栈不为空，说明有父符号
            if (!symbolStack.isEmpty()) {
                // 这是一个子符号，不应该被单独提取
                subSymbolNames.insert(info.name);
                qDebug() << "Found sub-symbol (skipping):" << info.name << "inside parent" << symbolStack.last().name;
            } else {
                // 这是一个顶层符号，提取它
                QString symbolContent;
                for (int k = info.startLine; k <= i; ++k) {
                    symbolContent += lines[k] + "\n";
                }
                existingSymbols[info.name] = symbolContent;
                qDebug() << "Extracted top-level symbol:" << info.name << "from lines" << info.startLine << "-" << i;
            }
        }
    }

    qDebug() << "Existing symbols count:" << existingSymbols.count();
    qDebug() << "Sub-symbol names:" << subSymbolNames;

    // 确定要导出的符号
    QList<IR::SymbolComponentIR> symbolsToExport;
    int overwriteCount = 0;
    int appendCount = 0;
    int skipCount = 0;

    for (const IR::SymbolComponentIR& symbol : symbols) {
        QString symbolName = symbol.name;

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
            // 新符号
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

    // 收集要被覆盖的符号名
    QSet<QString> overwrittenSymbolNames;
    for (const IR::SymbolComponentIR& symbol : symbolsToExport) {
        overwrittenSymbolNames.insert(symbol.name);
    }

    // 收集要被删除的子符号名称（属于被覆盖的父符号的子符号）
    QSet<QString> subSymbolsToDelete;

    // 首先分析现有符号，确定哪些是分体式符号（有多个子符号）
    QMap<QString, QStringList> parentToSubSymbols;  // 父符号名 -> 子符号列
    for (const QString& subSymbolName : subSymbolNames) {
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
            // 这是一个单体符号，查找其子符号
            QString expectedSubSymbolName_v1 = parentSymbolName + "_0_1";
            QString expectedSubSymbolName_v2 = parentSymbolName + "_1_1";
            if (subSymbolNames.contains(expectedSubSymbolName_v1)) {
                subSymbolsToDelete.insert(expectedSubSymbolName_v1);
                qDebug() << "Marking sub-symbol for deletion (single part v1):" << expectedSubSymbolName_v1
                         << "(parent:" << parentSymbolName << ")";
            } else if (subSymbolNames.contains(expectedSubSymbolName_v2)) {
                subSymbolsToDelete.insert(expectedSubSymbolName_v2);
                qDebug() << "Marking sub-symbol for deletion (single part v2):" << expectedSubSymbolName_v2
                         << "(parent:" << parentSymbolName << ")";
            }
        }
    }

    // 先导出未覆盖的现有符号（需要过滤掉被覆盖的子符号和以被覆盖符号名开头的顶层符号）
    for (const QString& symbolName : existingSymbols.keys()) {
        bool isOverwritten = overwrittenSymbolNames.contains(symbolName);

        // 检查是否是以被覆盖符号名开头的顶层符号
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
            // 导出未覆盖的符号，但需要过滤掉子符号
            QString symbolContent = existingSymbols[symbolName];
            QStringList contentLines = symbolContent.split('\n');
            QString filteredContent;
            bool skipNextSymbol = false;
            int nestedBraceCount = 0;

            for (const QString& line : contentLines) {
                QString trimmedLine = line.trimmed();

                // 检查是否是子符号定义的开始
                if (trimmedLine.startsWith("(symbol \"")) {
                    int nameStart = trimmedLine.indexOf("\"") + 1;
                    int nameEnd = trimmedLine.indexOf("\"", nameStart);
                    if (nameEnd > nameStart) {
                        QString subSymbolName = trimmedLine.mid(nameStart, nameEnd - nameStart);
                        if (subSymbolsToDelete.contains(subSymbolName)) {
                            skipNextSymbol = true;
                            nestedBraceCount = 1;
                            qDebug() << "Skipping deleted sub-symbol:" << subSymbolName;
                            continue;
                        }
                    }
                }

                if (skipNextSymbol) {
                    // 计算括号数量
                    for (int j = 0; j < line.length(); ++j) {
                        if (line[j] == '(')
                            nestedBraceCount++;
                        else if (line[j] == ')') {
                            nestedBraceCount--;
                            if (nestedBraceCount == 0) {
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

    // 再导出新符号和被覆盖的符号
    for (const IR::SymbolComponentIR& symbol : symbolsToExport) {
        qDebug() << "Exporting symbol" << (++index) << "of" << symbolsToExport.count() << ":" << symbol.name;
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
    QString version = m_detectedVersion.isEmpty() ? "20211014" : m_detectedVersion;
    QString header = QString(
                         "(kicad_symbol_lib\n"
                         "  (version %1)\n"
                         "  (generator %2)\n")
                         .arg(version, KiCadExportMetadata::generatorName());

    return header;
}

QString ExporterSymbol::generateSymbolContent(const IR::SymbolComponentIR& symbol, const QString& libName) const {
    QString content;

    // V6 格式 - 主符号定义（包含属性）
    QString cleanSymbolName = symbol.name;
    content += QString("  (symbol \"%1\"\n").arg(cleanSymbolName);
    content += "    (in_bom yes)\n";
    content += "    (on_board yes)\n";

    // 找到最左上角的引脚（x最小且y最大），将其位置作为新的坐标原点
    // 注意：KiCad Y 轴已翻转，"最上方"对应最大 Y 值
    double originX = 0.0;
    double originY = 0.0;
    const QList<IR::SymbolPinIR>& pins = symbol.pins;
    if (!pins.isEmpty()) {
        originX = pins.first().position.x();
        originY = pins.first().position.y();
        for (const IR::SymbolPinIR& pin : pins) {
            if (pin.position.x() < originX || (pin.position.x() == originX && pin.position.y() > originY)) {
                originX = pin.position.x();
                originY = pin.position.y();
            }
        }
        qDebug() << "Top-left pin at:" << originX << originY;
    }

    // 设置原点偏移，使所有图形元素相对于该点定位
    m_graphicsGenerator.setCurrentOrigin(originX, originY);

    // 计算所有图形元素的实际边界，用于文本左右居中对齐
    // 多部件符号的属性使用默认值，不计算图形边界
    double graphCenterOffsetX = 0.0;
    double yHigh = 2.54;  // 默认值：100mil
    double yLow = -2.54;  // 默认值：-100mil

    if (!symbol.isMultiPart()) {
        double minX = std::numeric_limits<double>::max();
        double maxX = -std::numeric_limits<double>::max();
        auto updateBounds = [&](double x, double w) {
            minX = qMin(minX, x);
            maxX = qMax(maxX, x + w);
        };
        for (const auto& rect : symbol.rectangles) {
            minX = qMin(minX, rect.x0);
            maxX = qMax(maxX, rect.x0);
            minX = qMin(minX, rect.x1);
            maxX = qMax(maxX, rect.x1);
        }
        for (const auto& circle : symbol.circles) {
            double r = circle.radius;
            minX = qMin(minX, circle.center.x() - r);
            maxX = qMax(maxX, circle.center.x() + r);
        }
        for (const auto& ellipse : symbol.ellipses) {
            minX = qMin(minX, ellipse.center.x() - ellipse.radiusX);
            maxX = qMax(maxX, ellipse.center.x() + ellipse.radiusX);
        }
        for (const auto& arc : symbol.arcs) {
            minX = qMin(minX, arc.startPoint.x());
            maxX = qMax(maxX, arc.startPoint.x());
            minX = qMin(minX, arc.midPoint.x());
            maxX = qMax(maxX, arc.midPoint.x());
            minX = qMin(minX, arc.endPoint.x());
            maxX = qMax(maxX, arc.endPoint.x());
        }
        for (const auto& polyline : symbol.polylines) {
            for (const QPointF& pt : polyline.points) {
                minX = qMin(minX, pt.x());
                maxX = qMax(maxX, pt.x());
            }
        }
        for (const auto& polygon : symbol.polygons) {
            for (const QPointF& pt : polygon.points) {
                minX = qMin(minX, pt.x());
                maxX = qMax(maxX, pt.x());
            }
        }
        for (const auto& text : symbol.texts) {
            minX = qMin(minX, text.position.x());
            maxX = qMax(maxX, text.position.x());
        }
        for (const auto& pin : pins) {
            minX = qMin(minX, pin.position.x());
            maxX = qMax(maxX, pin.position.x());
        }
        // 如果没有任何图形元素，使用默认宽度
        double graphWidth = (minX <= maxX) ? (maxX - minX) : 0.0;
        if (minX <= maxX) {
            graphCenterOffsetX = minX + graphWidth / 2.0 - originX;
        }
        qDebug() << "Graph bounds - minX:" << minX << "maxX:" << maxX << "width:" << graphWidth
                 << "centerOffsetX(mm):" << graphCenterOffsetX;

        // 计算 y_high 和 y_low（坐标已在 KiCad 空间，无需再次翻转）
        // 综合引脚和图形元素的 Y 坐标
        if (!pins.isEmpty()) {
            for (const IR::SymbolPinIR& pin : pins) {
                double pinY = pin.position.y() - originY;
                yHigh = qMax(yHigh, pinY);
                yLow = qMin(yLow, pinY);
            }
        }
        // 图形元素也参与 Y 边界计算
        for (const auto& rect : symbol.rectangles) {
            yHigh = qMax(yHigh, rect.y0 - originY);
            yLow = qMin(yLow, rect.y0 - originY);
            yHigh = qMax(yHigh, rect.y1 - originY);
            yLow = qMin(yLow, rect.y1 - originY);
        }
        for (const auto& circle : symbol.circles) {
            double r = circle.radius;
            yHigh = qMax(yHigh, circle.center.y() + r - originY);
            yLow = qMin(yLow, circle.center.y() - r - originY);
        }
    }

    // 生成属性 — 所有属性放在图形下方，水平居中，垂直统一对齐
    double propertyY = yLow - 3.81;  // 图形下方固定偏移（150mil）
    double fontSize = 1.27;  // PROPERTY_FONT_SIZE

    // 辅助函数：转义属性值
    auto escapePropertyValue = [](const QString& value) -> QString {
        QString escaped = value;
        escaped.replace("\"", "\\\"");
        escaped.replace("\n", " ");
        escaped.replace("\t", " ");
        return escaped.trimmed();
    };

    // Reference 属性（图形下方居中）
    QString refPrefix = symbol.designatorPrefix;
    refPrefix.replace("?", "");
    content += QString("    (property\n");
    content += QString("      \"Reference\"\n");
    content += QString("      \"%1\"\n").arg(escapePropertyValue(refPrefix));
    content += "      (id 0)\n";
    content += QString("      (at %1 %2 0)\n").arg(graphCenterOffsetX, 0, 'f', 2).arg(propertyY, 0, 'f', 2);
    content += QString("      (effects (font (size %1 %2) (thickness 0) ) )\n")
                   .arg(fontSize, 0, 'f', 2)
                   .arg(fontSize, 0, 'f', 2);
    content += "    )\n";

    // Value 属性（与 Reference 同一高度，居中对齐）
    content += QString("    (property\n");
    content += QString("      \"Value\"\n");
    content += QString("      \"%1\"\n").arg(escapePropertyValue(symbol.name));
    content += "      (id 1)\n";
    content += QString("      (at %1 %2 0)\n").arg(graphCenterOffsetX, 0, 'f', 2).arg(propertyY, 0, 'f', 2);
    content += QString("      (effects (font (size %1 %2) (thickness 0) ) hide)\n")
                   .arg(fontSize, 0, 'f', 2)
                   .arg(fontSize, 0, 'f', 2);
    content += "    )\n";

    // Footprint 属性（隐藏，同一高度）
    if (!symbol.footprintName.isEmpty()) {
        content += QString("    (property\n");
        content += QString("      \"Footprint\"\n");
        QString footprintPath = QString("%1:%2").arg(libName, symbol.footprintName);
        content += QString("      \"%1\"\n").arg(escapePropertyValue(footprintPath));
        content += "      (id 2)\n";
        content += QString("      (at %1 %2 0)\n").arg(graphCenterOffsetX, 0, 'f', 2).arg(propertyY, 0, 'f', 2);
        content += QString("      (effects (font (size %1 %2) (thickness 0) ) hide)\n")
                       .arg(fontSize, 0, 'f', 2)
                       .arg(fontSize, 0, 'f', 2);
        content += "    )\n";
    }

    // LCSC Part 属性（隐藏，同一高度）
    const QString lcscId = symbol.sourceMetadata.value("lcscId");
    if (!lcscId.isEmpty()) {
        content += QString("    (property\n");
        content += QString("      \"LCSC Part\"\n");
        content += QString("      \"%1\"\n").arg(escapePropertyValue(lcscId));
        content += "      (id 5)\n";
        content += QString("      (at %1 %2 0)\n").arg(graphCenterOffsetX, 0, 'f', 2).arg(propertyY, 0, 'f', 2);
        content += QString("      (effects (font (size %1 %2) (thickness 0) ) hide)\n")
                       .arg(fontSize, 0, 'f', 2)
                       .arg(fontSize, 0, 'f', 2);
        content += "    )\n";
    }

    // ki_description 属性（隐藏，同一高度）
    const QString symbolDescription = symbol.description.trimmed();
    if (!symbolDescription.isEmpty()) {
        content += QString("    (property\n");
        content += QString("      \"ki_description\"\n");
        content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolDescription));
        content += "      (id 6)\n";
        content += QString("      (at %1 %2 0)\n").arg(graphCenterOffsetX, 0, 'f', 2).arg(propertyY, 0, 'f', 2);
        content += QString("      (effects (font (size %1 %2) (thickness 0) ) hide)\n")
                       .arg(fontSize, 0, 'f', 2)
                       .arg(fontSize, 0, 'f', 2);
        content += "    )\n";
    }

    // 检查是否为多部分符号
    bool isMultiPart = symbol.isMultiPart();
    qDebug() << "=== Symbol Type Check ===";
    qDebug() << "Symbol name:" << cleanSymbolName;
    qDebug() << "Is multi-part:" << isMultiPart;
    qDebug() << "Part count:" << symbol.partCount;

    if (isMultiPart) {
        // 多部分符号：为每个 part 生成子符号
        qDebug() << "Exporting multi-part symbol with" << symbol.partCount << "parts";

        for (int partIdx = 0; partIdx < symbol.partCount; ++partIdx) {
            qDebug() << "Generating sub-symbol for part" << partIdx + 1;

            // 子符号名称：_{partNumber}_1（partNumber 从 1 开始）
            content += QString("    (symbol \"%1_%2_1\"\n").arg(cleanSymbolName).arg(partIdx + 1);

            // 收集属于此 part 的引脚和图形
            QList<IR::SymbolPinIR> partPins;
            for (const auto& pin : pins) {
                if (pin.partIndex == partIdx) {
                    partPins.append(pin);
                }
            }

            // 设置原点为 part 的坐标原点（在 IR 中为0,0，因为转换器已减去 part 原点）
            double partOriginX = 0.0;
            double partOriginY = 0.0;
            m_graphicsGenerator.setCurrentOrigin(partOriginX, partOriginY);

            // 生成属于此 part 的图形元素
            content += generatePartDrawings(symbol, partIdx);

            // 生成属于此 part 的引脚
            content += m_graphicsGenerator.generatePins(partPins);

            content += "    )\n";  // 结束子符号
        }
    } else {
        // 单部分符号：直接在主符号中包含图形元素，不使用子符号
        qDebug() << "Exporting single-part symbol";

        // 生成图形元素
        content += m_graphicsGenerator.generateDrawings(symbol);

        // 生成引脚
        content += m_graphicsGenerator.generatePins(pins);
    }

    content += "  )\n";  // 结束主符号

    return content;
}

QString ExporterSymbol::generatePartDrawings(const IR::SymbolComponentIR& symbol, int partIdx) const {
    QString content;
    for (const auto& rect : symbol.rectangles) {
        if (rect.partIndex == partIdx)
            content += m_graphicsGenerator.generateRectangle(rect);
    }
    for (const auto& circle : symbol.circles) {
        if (circle.partIndex == partIdx)
            content += m_graphicsGenerator.generateCircle(circle);
    }
    for (const auto& arc : symbol.arcs) {
        if (arc.partIndex == partIdx)
            content += m_graphicsGenerator.generateArc(arc);
    }
    for (const auto& ellipse : symbol.ellipses) {
        if (ellipse.partIndex == partIdx)
            content += m_graphicsGenerator.generateEllipse(ellipse);
    }
    for (const auto& polygon : symbol.polygons) {
        if (polygon.partIndex == partIdx)
            content += m_graphicsGenerator.generatePolygon(polygon);
    }
    for (const auto& polyline : symbol.polylines) {
        if (polyline.partIndex == partIdx)
            content += m_graphicsGenerator.generatePolyline(polyline);
    }
    for (const auto& path : symbol.paths) {
        if (path.partIndex == partIdx)
            content += m_graphicsGenerator.generatePath(path);
    }
    for (const auto& text : symbol.texts) {
        if (text.partIndex == partIdx)
            content += m_graphicsGenerator.generateText(text);
    }
    return content;
}

}  // namespace EasyKiConverter
