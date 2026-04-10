#pragma once

#include "PtGraphAdvisor.h"

#include <QMainWindow>

class QComboBox;
class QLabel;
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

private:
    void loadMapHtml();
    static QString readApiKey();
    void setupCourseworkGraphDock();
    void reloadCourseworkGraphFromDefaultFiles();
    void fillCourseworkGraphStopCombos();

    QWebEngineView *m_view = nullptr;
    RouteService *m_routes = nullptr;
    TransportBridge *m_bridge = nullptr;

    PtGraphAdvisor m_ptGraph;
    QLabel *m_ptLoadStatus = nullptr;
    QComboBox *m_ptOriginCombo = nullptr;
    QComboBox *m_ptDestCombo = nullptr;
    QComboBox *m_ptPrefCombo = nullptr;
    QTextEdit *m_ptResults = nullptr;
};
