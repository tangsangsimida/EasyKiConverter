#include "CliConverter.h"

#include "BaseConverter.h"
#include "BatchConverter.h"
#include "BomConverter.h"
#include "CliContext.h"
#include "ComponentConverter.h"

namespace EasyKiConverter {

CliConverter::CliConverter(const CommandLineParser& parser, QObject* parent) : QObject(parent) {
    m_context = new CliContext(parser);
}

CliConverter::~CliConverter() {
    if (m_converter) {
        delete m_converter;
    }
    if (m_context) {
        delete m_context;
    }
}

bool CliConverter::execute() {
    CommandLineParser::CliMode mode = m_context->parser().cliMode();

    m_converter = createConverter(mode);
    if (!m_converter) {
        m_context->setErrorMessage("未知的 CLI 模式");
        return false;
    }

    return m_converter->execute();
}

QString CliConverter::errorMessage() const {
    if (m_converter) {
        return m_converter->errorMessage();
    }
    if (m_context) {
        return m_context->errorMessage();
    }
    return "未知错误";
}

BaseConverter* CliConverter::createConverter(CommandLineParser::CliMode mode) {
    switch (mode) {
        case CommandLineParser::CliMode::ConvertBom:
            return new BomConverter(m_context, this);
        case CommandLineParser::CliMode::ConvertComponent:
            return new ComponentConverter(m_context, this);
        case CommandLineParser::CliMode::ConvertBatch:
            return new BatchConverter(m_context, this);
        default:
            return nullptr;
    }
}

}  // namespace EasyKiConverter
