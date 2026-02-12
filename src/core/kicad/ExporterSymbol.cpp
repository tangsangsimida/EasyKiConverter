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

    // 生成符号内容（不包含库头，仅单个符号定义
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

        // 生成所有符
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

    // 读取现有库内
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
    QMap<QString, QString> existingSymbols;  // 符号-> 符号内容
    QSet<QString> subSymbolNames;            // 属于分体式符号的子符号名称

    // 使用更可靠的方法来提取符号：手动解析括号匹配
    QStringList lines = existingContent.split('\n');
    int symbolStart = -1;
    QString currentSymbolName;
    int braceCount = 0;
    int parentSymbolDepth = 0;  // 父符号的深度（用于识别子符号

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();

        // 检查是否是符号定义的开始
        if (line.startsWith("(symbol \"")) {
            // 提取符号
            int nameStart = line.indexOf("\"") + 1;
            int nameEnd = line.indexOf("\"", nameStart);
            if (nameEnd > nameStart) {
                QString symbolName = line.mid(nameStart, nameEnd - nameStart);

                if (symbolStart >= 0) {
                    // 这是一个嵌套的符号定义（子符号
                    subSymbolNames.insert(symbolName);
                    qDebug() << "Found sub-symbol:" << symbolName << "inside parent symbol";
                } else {
                    // 这是一个顶层符号定位
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
                    // 如果括号深度大于 1，说明我们在父符号内
                    if (braceCount > 1)
                        parentSymbolDepth++;
                } else if (line[j] == ')') {
                    braceCount--;
                    if (parentSymbolDepth > 0)
                        parentSymbolDepth--;
                }
            }

            // 当括号数量归零时，符号定义结
            if (braceCount == 0) {
                // 只保存顶层符号，不保存子符号
                QString symbolContent;
                for (int k = symbolStart; k <= i; ++k) {
                    symbolContent += lines[k] + "\n";
                }
                existingSymbols[currentSymbolName] = symbolContent;
                qDebug() << "Found existing symbol:" << currentSymbolName << "at lines" << symbolStart << "-" << i;

                // 重置状
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
            // 新符
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
    for (const SymbolData& symbol : symbolsToExport) {
        overwrittenSymbolNames.insert(symbol.info().name);
    }

    // 收集要被删除的子符号名称（属于被覆盖的父符号的子符号
    // 改进的逻辑：准确识别分体式符号的子符号
    QSet<QString> subSymbolsToDelete;

    // 首先分析现有符号，确定哪些是分体式符号（有多个子符号
    QMap<QString, QStringList> parentToSubSymbols;  // 父符号名 -> 子符号列
    for (const QString& subSymbolName : subSymbolNames) {
        // 从子符号名称中提取父符号
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
            // 这是一个单体符号，查找其子符号（格式：{parentName}_0_1）
            QString expectedSubSymbolName = parentSymbolName + "_0_1";
            if (subSymbolNames.contains(expectedSubSymbolName)) {
                subSymbolsToDelete.insert(expectedSubSymbolName);
                qDebug() << "Marking sub-symbol for deletion (single part):" << expectedSubSymbolName
                         << "(parent:" << parentSymbolName << ")";
            }
        }
    }

    // 先导出未覆盖的现有符号（需要过滤掉被覆盖的子符号和以被覆盖符号名开头的顶层符号
    for (const QString& symbolName : existingSymbols.keys()) {
        bool isOverwritten = overwrittenSymbolNames.contains(symbolName);

        // 检查是否是以被覆盖符号名开头的顶层符号（可能是之前错误导出的子符号
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
            QStringList lines = symbolContent.split('\n');
            QString filteredContent;
            bool skipNextSymbol = false;
            int braceCount = 0;

            for (const QString& line : lines) {
                QString trimmedLine = line.trimmed();

                // 检查是否是子符号定义的开始
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

    // 再导出新符号和被覆盖的符号
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
    // 注意：KiCad 6.x 允许符号名称中包含空格，所以不需要替换
    // 保持原始名称以确保更新模式下能正确匹配和替换
    content += QString("  (symbol \"%1\"\n").arg(cleanSymbolName);
    content += "    (in_bom yes)\n";
    content += "    (on_board yes)\n";

    // 设置当前边界框，用于图形元素的相对坐标计算
    SymbolBBox originalBBox = symbolData.bbox();
    SymbolBBox currentBBox = originalBBox;
    m_graphicsGenerator.setCurrentBBox(currentBBox);
    qDebug() << "BBox - x:" << currentBBox.x << "y:" << currentBBox.y << "width:" << currentBBox.width
             << "height:" << currentBBox.height;
    // 计算符号中心点（用于将符号居中显示）
    double centerX = currentBBox.x + currentBBox.width / 2.0;
    double centerY = currentBBox.y + currentBBox.height / 2.0;
    qDebug() << "Symbol center - centerX:" << centerX << "centerY:" << centerY;
    // 修改边界框，使其指向中心点，这样所有图形元素都会相对于中心点定位
    currentBBox.x = centerX;
    currentBBox.y = centerY;
    m_graphicsGenerator.setCurrentBBox(currentBBox);
    qDebug() << "Adjusted BBox for centering - x:" << currentBBox.x << "y:" << currentBBox.y;
    // 计算 y_high 和 y_low（使用引脚坐标，与 Python 版本保持一致）
    // 如果没有引脚，使用默认值以确保属性位置正
    double yHigh = 2.54;  // 默认值：100mil
    double yLow = -2.54;  // 默认值：-100mil
    QList<SymbolPin> pins = symbolData.pins();
    if (!pins.isEmpty()) {
        qDebug() << "Pin coordinates calculation:";
        for (const SymbolPin& pin : pins) {
            double pinY = -m_graphicsGenerator.pxToMm(pin.settings.posY - centerY);
            qDebug() << "  Pin" << pin.settings.spicePinNumber << "- raw Y:" << pin.settings.posY
                     << "converted Y:" << pinY;
            yHigh = qMax(yHigh, pinY);
            yLow = qMin(yLow, pinY);
        }
    } else {
        qDebug() << "No pins found, using default yHigh and yLow values";
    }
    qDebug() << "Final yHigh:" << yHigh << "yLow:" << yLow;

    // 生成属性
    double fieldOffset = 5.08;  // FIELD_OFFSET_START
    double fontSize = 1.27;     // PROPERTY_FONT_SIZE

    // 辅助函数：转义属性值
    auto escapePropertyValue = [](const QString& value) -> QString {
        QString escaped = value;
        escaped.replace("\"", "\\\"");  // 转义引号
        escaped.replace("\n", " ");     // 移除换行符
        escaped.replace("\t", " ");     // 移除制表符
        return escaped.trimmed();
    };

    // Reference 属性
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

    // Value 属性
    content += QString("    (property\n");
    content += QString("      \"Value\"\n");
    content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().name));
    content += "      (id 1)\n";
    content += QString("      (at 0 %1 0)\n").arg(yLow - fieldOffset, 0, 'f', 2);
    content += QString("      (effects (font (size %1 %2) (thickness 0) ) )\n")
                   .arg(fontSize, 0, 'f', 2)
                   .arg(fontSize, 0, 'f', 2);
    content += "    )\n";

    // Footprint 属性
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

    // Datasheet 属性
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

    // Manufacturer 属性
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

    // LCSC Part 属性
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

    // 检查是否为多部分符
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
        content += m_graphicsGenerator.generateDrawings(symbolData);

        // 生成引脚
        content += m_graphicsGenerator.generatePins(symbolData.pins(), m_graphicsGenerator.currentBBox());
    }

    content += "  )\n";  // 结束主符号

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
    // 单部分符号：使用 _0_1 作为子符号名称
    content += QString("    (symbol \"%1_0_1\"\n").arg(symbolName);
    // 生成图形元素（直接生成，不添加任何属性）
    content += m_graphicsGenerator.generateDrawings(symbolData);

    // 生成引脚（使用中心点计算坐标，让引脚也跟着图形一起移动）
    SymbolBBox centeredBBox;
    centeredBBox.x = centerX;
    centeredBBox.y = centerY;
    centeredBBox.width = symbolData.bbox().width;
    centeredBBox.height = symbolData.bbox().height;
    content += m_graphicsGenerator.generatePins(symbolData.pins(), centeredBBox);
    content += "    )\n";  // 结束子符号
    return content;
}

QString ExporterSymbol::generateSubSymbol(const SymbolData& symbolData,
                                          const SymbolPart& part,
                                          const QString& symbolName,
                                          const QString& libName,
                                          double centerX,
                                          double centerY) const {
    QString content;

    // 多部分符号：使用 _{unitNumber}_1 作为子符号名称
    // 注意：Unit 编号必须从1 开始，而不是从 0 开始
    content += QString("    (symbol \"%1_%2_1\"\n").arg(symbolName).arg(part.unitNumber + 1);

    // 计算子部分的边界框和中心点
    SymbolBBox partBBox = m_graphicsGenerator.calculatePartBBox(part);
    double partCenterX = partBBox.x + partBBox.width / 2.0;
    double partCenterY = partBBox.y + partBBox.height / 2.0;

    qDebug() << "Sub-symbol" << part.unitNumber << "- partCenterX:" << partCenterX << "partCenterY:" << partCenterY;

    // 临时保存当前的边界框
    SymbolBBox originalBBox = m_graphicsGenerator.currentBBox();

    // 设置边界框为子部分的中心点，以便图形元素使用正确的偏移
    SymbolBBox subBBox;
    subBBox.x = partCenterX;
    subBBox.y = partCenterY;
    subBBox.width = partBBox.width;
    subBBox.height = partBBox.height;
    m_graphicsGenerator.setCurrentBBox(subBBox);

    qDebug() << "Setting m_currentBBox for sub-symbol - x:" << subBBox.x << "y:" << subBBox.y;

    // 创建以子部分中心为基准的边界框（用于引脚）
    SymbolBBox centeredPartBBox;
    centeredPartBBox.x = partCenterX;
    centeredPartBBox.y = partCenterY;
    centeredPartBBox.width = partBBox.width;
    centeredPartBBox.height = partBBox.height;

    // 生成图形元素（直接生成，不添加任何属性）
    content += m_graphicsGenerator.generateDrawings(part);

    // 生成引脚（使用子部分的中心点进行偏移）
    content += m_graphicsGenerator.generatePins(part.pins, centeredPartBBox);

    // 恢复原始边界框
    m_graphicsGenerator.setCurrentBBox(originalBBox);

    content += "    )\n";  // 结束子符号

    return content;
}

}  // namespace EasyKiConverter
