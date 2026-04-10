#pragma once

#include <QMainWindow>

class QWebEngineView;
class TransportBridge;
class RouteService;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void loadMapHtml();
    static QString readApiKey();

    QWebEngineView *m_view = nullptr;
    RouteService *m_routes = nullptr;
    TransportBridge *m_bridge = nullptr;
};
