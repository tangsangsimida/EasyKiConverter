#include "BomParser.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

#include <xlsxdocument.h>

namespace EasyKiConverter {

namespace {

QStringList splitCsvLine(const QString& line) {
    QStringList cells;
    QString cell;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        const QChar character = line.at(i);
        if (character == QChar('"')) {
            if (inQuotes && i + 1 < line.size() && line.at(i + 1) == QChar('"')) {
                cell += QChar('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (character == QChar(',') && !inQuotes) {
            cells.append(cell);
            cell.clear();
        } else {
            cell += character;
        }
    }
    cells.append(cell);
    return cells;
}

bool csvRecordHasOpenQuote(const QString& record) {
    bool inQuotes = false;
    for (int i = 0; i < record.size(); ++i) {
        if (record.at(i) != QChar('"'))
            continue;
        if (inQuotes && i + 1 < record.size() && record.at(i + 1) == QChar('"')) {
            ++i;
        } else {
            inQuotes = !inQuotes;
        }
    }
    return inQuotes;
}

}  // namespace

BomParser::BomParser(QObject* parent) : QObject(parent) {}

const QSet<QString>& BomParser::getExcludedIds() {
    static const QSet<QString> excluded = {"C0402", "C0603", "C0805", "C1206"};
    return excluded;
}

bool BomParser::validateId(const QString& componentId) {
    // LCSC 元件ID格式：以 'C' 或 'c' 开头，后面跟至少4位数字
    static const QRegularExpression re("^[Cc]\\d{4,}$");
    bool matches = re.match(componentId).hasMatch();

    if (matches) {
        return !getExcludedIds().contains(componentId.toUpper());
    }
    return false;
}

QStringList BomParser::parse(const QString& filePath) {
    qDebug() << "BomParser: Parsing file:" << filePath;

    QStringList componentIds;
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists()) {
        qWarning() << "BomParser: File does not exist:" << filePath;
        return componentIds;
    }

    QString suffix = fileInfo.suffix().toLower();

    if (suffix == "csv" || suffix == "txt") {
        componentIds = parseCsv(filePath);
    } else if (suffix == "xlsx" || suffix == "xls") {
        componentIds = parseExcel(filePath);
    } else {
        qWarning() << "BomParser: Unsupported file format:" << suffix;
    }

    qDebug() << "BomParser: Extracted" << componentIds.size() << "IDs";
    return componentIds;
}

QStringList BomParser::parseCsv(const QString& filePath) {
    QStringList componentIds;
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "BomParser: Failed to open CSV:" << file.errorString();
        return componentIds;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    QString record;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (!record.isEmpty())
            record.append('\n');
        record.append(line);

        if (csvRecordHasOpenQuote(record))
            continue;

        const QStringList cells = splitCsvLine(record);
        for (const QString& cell : cells) {
            processCellText(cell, componentIds);
        }
        record.clear();
    }

    if (!record.isEmpty()) {
        const QStringList cells = splitCsvLine(record);
        for (const QString& cell : cells)
            processCellText(cell, componentIds);
    }

    file.close();
    return componentIds;
}

QStringList BomParser::parseExcel(const QString& filePath) {
    QStringList componentIds;
    QXlsx::Document xlsx(filePath);

    if (!xlsx.load()) {
        qWarning() << "BomParser: Failed to load Excel file";
        return componentIds;
    }

    for (const QString& sheetName : xlsx.sheetNames()) {
        xlsx.selectSheet(sheetName);
        QXlsx::CellRange range = xlsx.dimension();

        for (int row = range.firstRow(); row <= range.lastRow(); ++row) {
            for (int col = range.firstColumn(); col <= range.lastColumn(); ++col) {
                if (auto cell = xlsx.cellAt(row, col)) {
                    processCellText(cell->readValue().toString(), componentIds);
                }
            }
        }
    }

    return componentIds;
}

void BomParser::processCellText(const QString& text, QStringList& result) {
    QString trimmedCell = text.trimmed();

    // 去除可能的引号
    if (trimmedCell.startsWith('"') && trimmedCell.endsWith('"')) {
        trimmedCell = trimmedCell.mid(1, trimmedCell.length() - 2);
    }

    if (validateId(trimmedCell)) {
        QString id = trimmedCell.toUpper();
        if (!result.contains(id)) {
            result.append(id);
        }
    }
}

}  // namespace EasyKiConverter
