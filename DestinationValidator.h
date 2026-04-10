#pragma once

#include <QString>

struct DestinationValidation {
    bool ok = false;
    QString error;
    QString normalized;
};

class DestinationValidator {
public:
    static DestinationValidation validate(const QString &raw);
};
