#include "TransportBridge.h"

#include "DestinationValidator.h"
#include "RouteRanker.h"
#include "RouteService.h"

#include <QJsonDocument>

TransportBridge::TransportBridge(RouteService *routes, QObject *parent)
    : QObject(parent), m_routes(routes)
{
    connect(m_routes, &RouteService::geocodeFinished, this,
            [this](double lat, double lng) {
                m_routes->fetchTransitDirections(m_pendingOriginLat, m_pendingOriginLng, lat, lng);
            });
    connect(m_routes, &RouteService::geocodeFailed, this, [this](const QString &msg) {
        emit searchFailed(msg);
    });
    connect(m_routes, &RouteService::directionsFinished, this, [this](const QJsonDocument &doc) {
        const QJsonObject ranked = RouteRanker::buildRankedPayload(doc);
        emit searchCompleted(QString::fromUtf8(QJsonDocument(ranked).toJson(QJsonDocument::Compact)));
    });
    connect(m_routes, &RouteService::directionsFailed, this, [this](const QString &msg) {
        emit searchFailed(msg);
    });
}

QString TransportBridge::validateDestination(const QString &raw) const
{
    const DestinationValidation v = DestinationValidator::validate(raw);
    return v.ok ? QString() : v.error;
}

void TransportBridge::searchTransitRoutes(const QString &destination, double originLat, double originLng)
{
    const DestinationValidation v = DestinationValidator::validate(destination);
    if (!v.ok) {
        emit searchFailed(v.error);
        return;
    }
    if (originLat < -90.0 || originLat > 90.0 || originLng < -180.0 || originLng > 180.0) {
        emit searchFailed(QStringLiteral("Invalid origin coordinates. Use Locate me on the map."));
        return;
    }
    m_pendingOriginLat = originLat;
    m_pendingOriginLng = originLng;
    m_routes->geocodeAddress(v.normalized);
}

void TransportBridge::loadLocalRandomPreview()
{
    QString err;
    if (!m_local.ensureLoaded(&err)) {
        emit searchFailed(err);
        return;
    }
    const QJsonObject ranked = m_local.randomPreviewPayload();
    if (ranked.isEmpty()) {
        emit searchFailed(QStringLiteral("No bus routes could be built from CSV."));
        return;
    }
    emit searchCompleted(QString::fromUtf8(QJsonDocument(ranked).toJson(QJsonDocument::Compact)));
}

void TransportBridge::searchFromLocalCsv(const QString &destination, double originLat, double originLng)
{
    const DestinationValidation v = DestinationValidator::validate(destination);
    if (!v.ok) {
        emit searchFailed(v.error);
        return;
    }
    if (originLat < -90.0 || originLat > 90.0 || originLng < -180.0 || originLng > 180.0) {
        emit searchFailed(QStringLiteral("Invalid origin coordinates. Use Locate me on the map."));
        return;
    }
    QString err;
    if (!m_local.ensureLoaded(&err)) {
        emit searchFailed(err);
        return;
    }
    const QJsonObject ranked = m_local.suggestFromDestination(v.normalized, originLat, originLng);
    emit searchCompleted(QString::fromUtf8(QJsonDocument(ranked).toJson(QJsonDocument::Compact)));
}
