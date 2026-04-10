#include "TransportBridge.h"

#include "DestinationValidator.h"
#include "RouteRanker.h"
#include "RouteService.h"

#include <QJsonDocument>

TransportBridge::TransportBridge(RouteService *routes, QObject *parent)
    : QObject(parent), m_routes(routes)
{
    connect(m_routes, &RouteService::geocodeFinished, this, [this](double lat, double lng) {
        switch (m_geoStep) {
        case GeoStep::TransitNeedOrigin:
            m_oLat = lat;
            m_oLng = lng;
            m_haveO = true;
            m_geoStep = GeoStep::None;
            beginTransitGeocodeChain();
            return;
        case GeoStep::TransitNeedDest:
            m_dLat = lat;
            m_dLng = lng;
            m_haveD = true;
            m_geoStep = GeoStep::None;
            beginTransitGeocodeChain();
            return;
        case GeoStep::CsvNeedOrigin:
            m_oLat = lat;
            m_oLng = lng;
            m_geoStep = GeoStep::None;
            finishCsvSearchAfterGeocode();
            return;
        default:
            return;
        }
    });
    connect(m_routes, &RouteService::geocodeFailed, this, [this](const QString &msg) {
        m_geoStep = GeoStep::None;
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

QString TransportBridge::validateTripEndpoint(const QString &raw) const
{
    const TripEndpointParse p = DestinationValidator::parseTripEndpoint(raw);
    return p.ok ? QString() : p.error;
}

QString TransportBridge::validateDestination(const QString &raw) const
{
    return validateTripEndpoint(raw);
}

void TransportBridge::beginTransitGeocodeChain()
{
    if (!m_haveO) {
        m_geoStep = GeoStep::TransitNeedOrigin;
        m_routes->geocodeAddress(m_geocodeOriginText);
        return;
    }
    if (!m_haveD) {
        m_geoStep = GeoStep::TransitNeedDest;
        m_routes->geocodeAddress(m_geocodeDestText);
        return;
    }
    m_routes->fetchTransitDirections(m_oLat, m_oLng, m_dLat, m_dLng);
}

void TransportBridge::finishCsvSearchAfterGeocode()
{
    QString err;
    if (!m_local.ensureLoaded(&err)) {
        emit searchFailed(err);
        return;
    }
    const QJsonObject ranked = m_local.suggestFromDestination(m_csvDestNormalized, m_oLat, m_oLng);
    emit searchCompleted(QString::fromUtf8(QJsonDocument(ranked).toJson(QJsonDocument::Compact)));
}

void TransportBridge::searchTransitRoutes(const QString &originRaw, const QString &destRaw)
{
    m_geoStep = GeoStep::None;
    m_haveO = false;
    m_haveD = false;

    const TripEndpointParse po = DestinationValidator::parseTripEndpoint(originRaw);
    const TripEndpointParse pd = DestinationValidator::parseTripEndpoint(destRaw);
    if (!po.ok) {
        emit searchFailed(QStringLiteral("Starting point: %1").arg(po.error));
        return;
    }
    if (!pd.ok) {
        emit searchFailed(QStringLiteral("Destination: %1").arg(pd.error));
        return;
    }

    if (po.isLatLng) {
        m_oLat = po.lat;
        m_oLng = po.lng;
        m_haveO = true;
    } else {
        m_geocodeOriginText = po.normalizedAddress;
    }
    if (pd.isLatLng) {
        m_dLat = pd.lat;
        m_dLng = pd.lng;
        m_haveD = true;
    } else {
        m_geocodeDestText = pd.normalizedAddress;
    }

    beginTransitGeocodeChain();
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

void TransportBridge::searchFromLocalCsv(const QString &originRaw, const QString &destRaw)
{
    m_geoStep = GeoStep::None;

    const TripEndpointParse po = DestinationValidator::parseTripEndpoint(originRaw);
    const TripEndpointParse pd = DestinationValidator::parseTripEndpoint(destRaw);
    if (!po.ok) {
        emit searchFailed(QStringLiteral("Starting point: %1").arg(po.error));
        return;
    }
    if (!pd.ok) {
        emit searchFailed(QStringLiteral("Destination: %1").arg(pd.error));
        return;
    }
    if (pd.isLatLng) {
        emit searchFailed(QStringLiteral(
            "Plan from CSV needs a destination name to match bus stops (not latitude, longitude)."));
        return;
    }

    QString err;
    if (!m_local.ensureLoaded(&err)) {
        emit searchFailed(err);
        return;
    }

    if (po.isLatLng) {
        const QJsonObject ranked = m_local.suggestFromDestination(pd.normalizedAddress, po.lat, po.lng);
        emit searchCompleted(QString::fromUtf8(QJsonDocument(ranked).toJson(QJsonDocument::Compact)));
        return;
    }

    if (m_routes == nullptr) {
        emit searchFailed(QStringLiteral("Internal error."));
        return;
    }
    m_csvDestNormalized = pd.normalizedAddress;
    m_geoStep = GeoStep::CsvNeedOrigin;
    m_routes->geocodeAddress(po.normalizedAddress);
}
