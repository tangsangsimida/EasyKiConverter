#include "BaseConverter.h"

#include "CliContext.h"
#include "CliPrinter.h"

namespace EasyKiConverter {

BaseConverter::BaseConverter(CliContext* context, QObject* parent)
    : QObject(parent), m_context(context), m_printer(new CliPrinter(context->parser().isQuietMode())) {}

void BaseConverter::printMessage(const QString& message) const {
    if (m_printer) {
        m_printer->println(message);
    }
}

void BaseConverter::printProgressBar(int progress) const {
    if (m_printer) {
        m_printer->printProgressBar(progress);
    }
}

}  // namespace EasyKiConverter
