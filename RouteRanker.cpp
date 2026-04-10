#include "RouteRanker.h"

#include <algorithm>

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QRegularExpression>

namespace {

int countTransitLegs(const QJsonObject &route)
{
    int n = 0;
    const QJsonArray legs = route.value(QStringLiteral("legs")).toArray();
    for (const QJsonValue &lv : legs) {
        const QJsonArray steps = lv.toObject().value(QStringLiteral("steps")).toArray();
        for (const QJsonValue &sv : steps) {
            const QString mode = sv.toObject().value(QStringLiteral("travel_mode")).toString();
            if (mode == QLatin1String("TRANSIT"))
                ++n;
        }
    }
    return n;
}

double parseFareToHkd(const QString &text)
{
    static const QRegularExpression re(QStringLiteral(R"((\d+(?:\.\d+)?))"));
    const QRegularExpressionMatch m = re.match(text);
    if (m.hasMatch())
        return m.captured(1).toDouble();
    return -1.0;
}

QJsonObject routeToJson(const QJsonObject &route)
{
    const QJsonArray legs = route.value(QStringLiteral("legs")).toArray();
    int durationSec = 0;
    QString durationText;
    QString startAddr;
    QString endAddr;
    for (const QJsonValue &lv : legs) {
        const QJsonObject leg = lv.toObject();
        durationSec += leg.value(QStringLiteral("duration")).toObject().value(QStringLiteral("value")).toInt();
        durationText = leg.value(QStringLiteral("duration")).toObject().value(QStringLiteral("text")).toString();
        if (startAddr.isEmpty())
            startAddr = leg.value(QStringLiteral("start_address")).toString();
        endAddr = leg.value(QStringLiteral("end_address")).toString();
    }
    QString fareText;
    double fareValue = -1.0;
    if (route.contains(QStringLiteral("fare"))) {
        const QJsonObject fare = route.value(QStringLiteral("fare")).toObject();
        fareText = fare.value(QStringLiteral("text")).toString();
        if (fareText.isEmpty())
            fareText = fare.value(QStringLiteral("value")).toString();
        fareValue = parseFareToHkd(fareText);
    }
    const int transfers = countTransitLegs(route);
    const QString poly = route.value(QStringLiteral("overview_polyline")).toObject().value(QStringLiteral("points")).toString();
    const QString summary = route.value(QStringLiteral("summary")).toString();

    QJsonObject o;
    o.insert(QStringLiteral("summary"), summary);
    o.insert(QStringLiteral("durationSec"), durationSec);
    o.insert(QStringLiteral("durationText"), durationText);
    o.insert(QStringLiteral("fareText"), fareText.isEmpty() ? QStringLiteral("Not provided") : fareText);
    o.insert(QStringLiteral("fareSort"), fareValue >= 0 ? fareValue : 1.0e9);
    o.insert(QStringLiteral("transfers"), transfers);
    o.insert(QStringLiteral("polyline"), poly);
    o.insert(QStringLiteral("startAddress"), startAddr);
    o.insert(QStringLiteral("endAddress"), endAddr);
    return o;
}

QJsonArray sortByNumberKey(const QJsonArray &in, const QString &key, bool ascending)
{
    QList<QJsonObject> rows;
    rows.reserve(in.size());
    for (const QJsonValue &v : in)
        rows.append(v.toObject());
    std::sort(rows.begin(), rows.end(), [&](const QJsonObject &a, const QJsonObject &b) {
        const double va = a.value(key).toDouble();
        const double vb = b.value(key).toDouble();
        return ascending ? (va < vb) : (va > vb);
    });
    QJsonArray out;
    for (const QJsonObject &o : rows)
        out.append(o);
    return out;
}

} // namespace

QJsonObject RouteRanker::buildRankedPayload(const QJsonDocument &directionsDoc)
{
    const QJsonObject root = directionsDoc.object();
    const QJsonArray routesIn = root.value(QStringLiteral("routes")).toArray();
    QJsonArray base;
    for (const QJsonValue &rv : routesIn)
        base.append(routeToJson(rv.toObject()));

    QJsonObject out;
    out.insert(QStringLiteral("byTime"), sortByNumberKey(base, QStringLiteral("durationSec"), true));
    out.insert(QStringLiteral("byCost"), sortByNumberKey(base, QStringLiteral("fareSort"), true));
    out.insert(QStringLiteral("byTransfers"), sortByNumberKey(base, QStringLiteral("transfers"), true));
    out.insert(QStringLiteral("rawRouteCount"), routesIn.size());
    return out;
}
