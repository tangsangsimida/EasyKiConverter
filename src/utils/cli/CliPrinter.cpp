#include "CliPrinter.h"

#include <QTextStream>

namespace EasyKiConverter {

CliPrinter::CliPrinter(bool quiet) : m_quiet(quiet) {}

void CliPrinter::print(const QString& message) const {
    if (!m_quiet) {
        QTextStream(stdout) << message;
    }
}

void CliPrinter::println(const QString& message) const {
    if (!m_quiet) {
        QTextStream(stdout) << message << Qt::endl;
    }
}

void CliPrinter::printError(const QString& message) const {
    QTextStream(stderr) << message << Qt::endl;
}

void CliPrinter::printProgressBar(int progress) const {
    if (m_quiet) {
        return;
    }

    const int barWidth = 50;
    int pos = barWidth * progress / 100;

    QTextStream stream(stdout);
    stream << "\r[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) {
            stream << "=";
        } else if (i == pos) {
            stream << ">";
        } else {
            stream << " ";
        }
    }
    stream << "] " << progress << "%";
    stream.flush();
}

void CliPrinter::clearLine() const {
    if (!m_quiet) {
        QTextStream(stdout) << "\r\033[K";
    }
}

}  // namespace EasyKiConverter
