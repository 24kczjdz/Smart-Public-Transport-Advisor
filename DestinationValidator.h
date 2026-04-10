#pragma once

#include <QString>

struct DestinationValidation {
    bool ok = false;
    QString error;
    QString normalized;
};

/** Address text (same rules as validate) or a comma-separated latitude,longitude pair. */
struct TripEndpointParse {
    bool ok = false;
    QString error;
    bool isLatLng = false;
    double lat = 0.0;
    double lng = 0.0;
    QString normalizedAddress;
};

class DestinationValidator {
public:
    static DestinationValidation validate(const QString &raw);
    static TripEndpointParse parseTripEndpoint(const QString &raw);
};
