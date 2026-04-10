#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

class RouteRanker {
public:
    static QJsonObject buildRankedPayload(const QJsonDocument &directionsDoc);
};
