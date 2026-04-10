#pragma once

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class RouteService : public QObject {
    Q_OBJECT
public:
    explicit RouteService(QObject *parent = nullptr);

    void setApiKey(const QString &key) { m_apiKey = key; }

    void geocodeAddress(const QString &address);
    void fetchTransitDirections(double originLat, double originLng, double destLat, double destLng);

signals:
    void geocodeFinished(double lat, double lng);
    void geocodeFailed(const QString &message);
    void directionsFinished(const QJsonDocument &doc);
    void directionsFailed(const QString &message);

private:
    QNetworkAccessManager m_nam;
    QString m_apiKey;
};
