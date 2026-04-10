#include "RouteService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrlQuery>

namespace {

QUrl geocodeUrl(const QString &address, const QString &key)
{
    QUrl u(QStringLiteral("https://maps.googleapis.com/maps/api/geocode/json"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("address"), address);
    q.addQueryItem(QStringLiteral("key"), key);
    u.setQuery(q);
    return u;
}

QUrl directionsUrl(double olat, double olng, double dlat, double dlng, const QString &key)
{
    QUrl u(QStringLiteral("https://maps.googleapis.com/maps/api/directions/json"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("origin"),
                   QString::number(olat, 'f', 7) + QLatin1Char(',') + QString::number(olng, 'f', 7));
    q.addQueryItem(QStringLiteral("destination"),
                   QString::number(dlat, 'f', 7) + QLatin1Char(',') + QString::number(dlng, 'f', 7));
    q.addQueryItem(QStringLiteral("mode"), QStringLiteral("transit"));
    q.addQueryItem(QStringLiteral("alternatives"), QStringLiteral("true"));
    q.addQueryItem(QStringLiteral("key"), key);
    u.setQuery(q);
    return u;
}

} // namespace

RouteService::RouteService(QObject *parent) : QObject(parent)
{
    connect(&m_nam, &QNetworkAccessManager::finished, this, [this](QNetworkReply *reply) {
        const QString kind = reply->property("transportAdvisorKind").toString();
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (kind == QLatin1String("geocode"))
                emit geocodeFailed(reply->errorString());
            else if (kind == QLatin1String("directions"))
                emit directionsFailed(reply->errorString());
            return;
        }
        const QByteArray body = reply->readAll();
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            const QString msg = QStringLiteral("Invalid JSON from Google APIs.");
            if (kind == QLatin1String("geocode"))
                emit geocodeFailed(msg);
            else if (kind == QLatin1String("directions"))
                emit directionsFailed(msg);
            return;
        }
        const QJsonObject root = doc.object();
        const QString status = root.value(QStringLiteral("status")).toString();
        if (status != QLatin1String("OK")) {
            const QString msg = root.value(QStringLiteral("error_message")).toString();
            const QString fallback = QStringLiteral("API status: %1").arg(status);
            const QString out = msg.isEmpty() ? fallback : msg;
            if (kind == QLatin1String("geocode"))
                emit geocodeFailed(out);
            else if (kind == QLatin1String("directions"))
                emit directionsFailed(out);
            return;
        }
        if (kind == QLatin1String("geocode")) {
            const QJsonArray results = root.value(QStringLiteral("results")).toArray();
            if (results.isEmpty()) {
                emit geocodeFailed(QStringLiteral("No results for that address."));
                return;
            }
            const QJsonObject loc = results.at(0).toObject().value(QStringLiteral("geometry")).toObject()
                                        .value(QStringLiteral("location"))
                                        .toObject();
            const double lat = loc.value(QStringLiteral("lat")).toDouble();
            const double lng = loc.value(QStringLiteral("lng")).toDouble();
            emit geocodeFinished(lat, lng);
            return;
        }
        if (kind == QLatin1String("directions")) {
            emit directionsFinished(doc);
            return;
        }
    });
}

void RouteService::geocodeAddress(const QString &address)
{
    if (m_apiKey.isEmpty()) {
        emit geocodeFailed(QStringLiteral("Missing Google API key. Set GOOGLE_MAPS_API_KEY or config.ini."));
        return;
    }
    QNetworkRequest req(geocodeUrl(address, m_apiKey));
    QNetworkReply *reply = m_nam.get(req);
    reply->setProperty("transportAdvisorKind", QStringLiteral("geocode"));
}

void RouteService::fetchTransitDirections(double originLat, double originLng, double destLat, double destLng)
{
    if (m_apiKey.isEmpty()) {
        emit directionsFailed(QStringLiteral("Missing Google API key."));
        return;
    }
    QNetworkRequest req(directionsUrl(originLat, originLng, destLat, destLng, m_apiKey));
    QNetworkReply *reply = m_nam.get(req);
    reply->setProperty("transportAdvisorKind", QStringLiteral("directions"));
}
