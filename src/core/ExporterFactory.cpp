#include "ExporterFactory.h"

#include "core/kicad/ExporterSymbol.h"
#include "core/kicad/ExporterFootprint.h"
#include "core/kicad/Exporter3DModel.h"

// Altium 导出器头文件（第二阶段实现后取消注释）
// #include "core/altium/ExporterAltiumSymbol.h"
// #include "core/altium/ExporterAltiumFootprint.h"
// #include "core/altium/ExporterAltium3DModel.h"

namespace EasyKiConverter {

/**
 * @brief 创建符号导出器
 */
std::unique_ptr<ISymbolExporter> ExporterFactory::createSymbolExporter(TargetEdaFormat format) {
    switch (format) {
    case TargetEdaFormat::KiCad:
        return std::make_unique<ExporterSymbol>();
    case TargetEdaFormat::Altium:
        // TODO(第二阶段): 实现 Altium 符号导出器后替换
        // return std::make_unique<ExporterAltiumSymbol>();
        return nullptr;
    default:
        return nullptr;
    }
}

/**
 * @brief 创建封装导出器
 */
std::unique_ptr<IFootprintExporter> ExporterFactory::createFootprintExporter(TargetEdaFormat format) {
    switch (format) {
    case TargetEdaFormat::KiCad:
        return std::make_unique<ExporterFootprint>();
    case TargetEdaFormat::Altium:
        // TODO(第二阶段): 实现 Altium 封装导出器后替换
        // return std::make_unique<ExporterAltiumFootprint>();
        return nullptr;
    default:
        return nullptr;
    }
}

/**
 * @brief 创建 3D 模型导出器
 * @note 使用裸 new 而非 std::make_unique，因为 Exporter3DModel 继承 QObject，
 *       需要将 parent 传递给构造函数以建立 Qt 对象所有权。
 */
std::unique_ptr<IModel3DExporter> ExporterFactory::createModel3DExporter(TargetEdaFormat format,
                                                                         QObject* parent) {
    switch (format) {
    case TargetEdaFormat::KiCad:
        return std::unique_ptr<IModel3DExporter>(new Exporter3DModel(parent));
    case TargetEdaFormat::Altium:
        // TODO(第二阶段): Altium 同样支持 WRL/STEP，届时决定复用 KiCad 实现还是提供专用版本
        return nullptr;
    default:
        return nullptr;
    }
}

}  // namespace EasyKiConverter
