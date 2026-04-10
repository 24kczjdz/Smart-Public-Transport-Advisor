#pragma once

#include "LocalBusCsvModel.h"

#include <QJsonDocument>
#include <QObject>
#include <QString>

class RouteService;

class TransportBridge : public QObject {
    Q_OBJECT
public:
    explicit TransportBridge(RouteService *routes, QObject *parent = nullptr);

    void setLocalCsvDirectory(const QString &absoluteDir) { m_local.setCsvDirectory(absoluteDir); }

    Q_INVOKABLE QString validateDestination(const QString &raw) const;
    /** Accepts place text or "lat, lng" (e.g. filled by Locate me). */
    Q_INVOKABLE QString validateTripEndpoint(const QString &raw) const;

    Q_INVOKABLE void searchTransitRoutes(const QString &origin, const QString &destination);

    Q_INVOKABLE void loadLocalRandomPreview();
    Q_INVOKABLE void searchFromLocalCsv(const QString &origin, const QString &destination);

signals:
    void searchCompleted(const QString &jsonPayload);
    void searchFailed(const QString &message);

private:
    enum class GeoStep { None, TransitNeedOrigin, TransitNeedDest, CsvNeedOrigin };

    void beginTransitGeocodeChain();
    void finishCsvSearchAfterGeocode();

    RouteService *m_routes = nullptr;
    LocalBusCsvModel m_local;
    GeoStep m_geoStep = GeoStep::None;
    bool m_haveO = false;
    bool m_haveD = false;
    double m_oLat = 0.0;
    double m_oLng = 0.0;
    double m_dLat = 0.0;
    double m_dLng = 0.0;
    QString m_geocodeOriginText;
    QString m_geocodeDestText;
    QString m_csvDestNormalized;
};
