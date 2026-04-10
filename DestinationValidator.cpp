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

TripEndpointParse DestinationValidator::parseTripEndpoint(const QString &raw)
{
    TripEndpointParse out;
    const QString s = raw.trimmed();
    if (s.isEmpty()) {
        out.error = QStringLiteral("Cannot be empty.");
        return out;
    }
    const int comma = s.indexOf(QLatin1Char(','));
    if (comma > 0 && comma < s.size() - 1) {
        bool okLat = false;
        bool okLng = false;
        const double lat = s.left(comma).trimmed().toDouble(&okLat);
        const double lng = s.mid(comma + 1).trimmed().toDouble(&okLng);
        if (okLat && okLng && lat >= -90.0 && lat <= 90.0 && lng >= -180.0 && lng <= 180.0) {
            out.ok = true;
            out.isLatLng = true;
            out.lat = lat;
            out.lng = lng;
            return out;
        }
    }
    const DestinationValidation v = validate(s);
    if (!v.ok) {
        out.error = v.error;
        return out;
    }
    out.ok = true;
    out.isLatLng = false;
    out.normalizedAddress = v.normalized;
    return out;
}
