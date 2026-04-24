#pragma once

#include "PtGraphAdvisor.h"

#include <QMainWindow>

class QComboBox;
class QLabel;
class QPushButton;
class QTextEdit;
class QWebEngineView;
class TransportBridge;
class RouteService;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onCourseworkGraphFind();
    void onCourseworkGraphLoadSelectedNetwork();
    void onCourseworkRoutePicked(int index);

private:
    void loadMapHtml();
    static QString readApiKey();
    void setupCourseworkGraphDock();
    void reloadCourseworkGraphFromDefaultFiles();
    void fillCourseworkGraphStopCombos();
    void syncCourseworkNetworkOverlayOnMap();
    void showCourseworkJourneyOnMap(const PtJourney &journey, const QString &originId);
    void setRouteButtonState(int index, bool enabled, const QString &text);

    QWebEngineView *m_view = nullptr;
    RouteService *m_routes = nullptr;
    TransportBridge *m_bridge = nullptr;

    PtGraphAdvisor m_ptGraph;
    QLabel *m_ptLoadStatus = nullptr;
    QComboBox *m_ptNetworkCombo = nullptr;
    QComboBox *m_ptOriginCombo = nullptr;
    QComboBox *m_ptDestCombo = nullptr;
    QComboBox *m_ptPrefCombo = nullptr;
    QPushButton *m_ptRouteBtn[3] = {nullptr, nullptr, nullptr};
    QTextEdit *m_ptResults = nullptr;
    QString m_ptLastOrigin;
    QVector<PtJourney> m_ptLastTopJourneys;
};
