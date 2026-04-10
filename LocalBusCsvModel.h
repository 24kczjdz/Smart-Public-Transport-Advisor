#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QVector>

struct RouteMeta {
    int routeDbId = 0;
    QString routeName;
    int journeyMinutes = 0;
    double fullFare = 0.0;
    QString terminusE;
};

struct RouteStopPt {
    int stopId = 0;
    QString nameEn;
};

class LocalBusCsvModel {
public:
    void setCsvDirectory(const QString &absoluteDir) { m_csvOverride = absoluteDir; }

    bool ensureLoaded(QString *errorMessage);
    QJsonObject randomPreviewPayload();
    QJsonObject suggestFromDestination(const QString &destQuery, double originLat, double originLng);

private:
    QString pickCsvDirectory() const;
    static QHash<QString, int> headerToIndex(const QStringList &header);
    static QStringList splitCsvLine(const QString &line);

    bool loadStops(const QString &path, QString *err);
    bool loadRouteMeta(const QString &path, QString *err);
    bool loadRstop(const QString &path, QString *err);

    QJsonObject buildRankedPayload(const QVector<QJsonObject> &routes) const;
    QJsonObject routeToJson(const RouteMeta &meta, const QVector<RouteStopPt> &slice, int segCount,
                            int estDurationSec) const;
    bool coordsForStop(int stopId, double *lat, double *lng) const;
    QJsonArray pathJsonForSlice(const QVector<RouteStopPt> &slice) const;

    bool m_loaded = false;
    QString m_csvOverride;
    QString m_csvDir;
    QHash<int, QPair<double, double>> m_stopLatLng; // STOP_ID → approx WGS84
    QHash<int, RouteMeta> m_routeById;              // ROUTE_ID from CSV
    QHash<QString, QVector<RouteStopPt>> m_stopsOnRoute; // "routeId:routeSeq" → ordered stops
    QMultiHash<int, QString> m_stopIdToRouteKeys;     // which routes serve a stop
};
