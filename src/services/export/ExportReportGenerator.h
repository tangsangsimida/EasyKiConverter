#pragma once

#include <QString>

namespace EasyKiConverter {

struct ExportOptions;
struct ExportOverallProgress;

class ExportReportGenerator {
public:
    static void logNetworkStats(const QString& context);

    static void writeDetailedReport(const QString& reason,
                                    const ExportOptions& options,
                                    const ExportOverallProgress& progress);
};

}  // namespace EasyKiConverter
