#ifndef DATASHEETEXPORTER_H
#define DATASHEETEXPORTER_H

#include "ExportService.h"

#include <QDir>
#include <QObject>
#include <QString>

namespace EasyKiConverter {

class DatasheetExporter : public QObject {
    Q_OBJECT
public:
    explicit DatasheetExporter(QObject* parent = nullptr);
    ~DatasheetExporter() override;

    void setOptions(const ExportOptions& options);

    bool exportDatasheet(const QString& datasheetUrl, const QString& outputPath, const QString& componentName);

signals:
    // 预留接口：当前为同步导出，暂时不使用这些信号
    // 将来如需改为非阻塞异步导出，可启用这些信号进行进度/完成通知
    void exportCompleted(bool success);
    void exportFailed(const QString& error);

private:
    bool createDatasheetDirectory(const QString& dirPath);

private:
    ExportOptions m_options;
};

}  // namespace EasyKiConverter

#endif  // DATASHEETEXPORTER_H
