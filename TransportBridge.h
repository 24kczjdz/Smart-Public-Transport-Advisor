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
    Q_INVOKABLE void searchTransitRoutes(const QString &destination, double originLat, double originLng);

    Q_INVOKABLE void loadLocalRandomPreview();
    Q_INVOKABLE void searchFromLocalCsv(const QString &destination, double originLat, double originLng);

signals:
    void searchCompleted(const QString &jsonPayload);
    void searchFailed(const QString &message);

private:
    RouteService *m_routes = nullptr;
    LocalBusCsvModel m_local;
    double m_pendingOriginLat = 0.0;
    double m_pendingOriginLng = 0.0;
};
