#include "CliContext.h"

#include "services/ComponentService.h"
#include "services/export/ParallelExportService.h"

namespace EasyKiConverter {

CliContext::CliContext(const CommandLineParser& parser) : m_parser(parser) {
    // 创建服务实例
    m_componentService = new ComponentService();
    m_exportService = new ParallelExportService();
    m_exportService->setComponentService(m_componentService);
}

CliContext::~CliContext() {
    if (m_exportService) {
        m_exportService->cancelExport();
        delete m_exportService;
    }
    if (m_componentService) {
        delete m_componentService;
    }
}

ExportOptions CliContext::createExportOptions() const {
    ExportOptions options;

    // 设置输出路径
    options.outputPath = m_parser.outputDir();

    // 设置导出类型（CLI 默认：符号库、封装库、3D模型-WRL格式）
    options.exportSymbol = m_parser.exportSymbol();
    options.exportFootprint = m_parser.exportFootprint();
    options.exportModel3D = m_parser.export3DModel();
    options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
    options.exportPreviewImages = m_parser.exportPreview();
    options.exportDatasheet = false;

    // 设置默认值
    options.overwriteExistingFiles = true;
    options.debugMode = m_parser.isDebugMode();

    return options;
}

}  // namespace EasyKiConverter
