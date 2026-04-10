#include "DestinationValidator.h"

#include <QRegularExpression>

DestinationValidation DestinationValidator::validate(const QString &raw)
{
    DestinationValidation out;
    const QString s = raw.trimmed();
    if (s.isEmpty()) {
        out.error = QStringLiteral("Destination cannot be empty.");
        return out;
    }
    if (s.length() < 3) {
        out.error = QStringLiteral("Please enter at least 3 characters.");
        return out;
    }
    if (s.length() > 200) {
        out.error = QStringLiteral("Destination is too long (max 200 characters).");
        return out;
    }
    static const QRegularExpression ctrl(QStringLiteral(R"([\x00-\x08\x0B\x0C\x0E-\x1F])"));
    if (s.indexOf(ctrl) >= 0) {
        out.error = QStringLiteral("Destination contains invalid control characters.");
        return out;
    }
    out.ok = true;
    out.normalized = s;
    return out;
}
