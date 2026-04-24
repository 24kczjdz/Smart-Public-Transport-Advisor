#include "MainWindow.h"

#include "PtGraphTypes.h"
#include "RouteService.h"
#include "TransportBridge.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QIODevice>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEngineView>

#include <algorithm>

namespace {

QString resolveCourseworkDataFile(const QString &relativeUnderDataCase1)
{
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../data/case1/%1").arg(relativeUnderDataCase1)),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/case1/%1").arg(relativeUnderDataCase1)),
        QDir::current().filePath(QStringLiteral("data/case1/%1").arg(relativeUnderDataCase1)),
        QDir::current().filePath(QStringLiteral("TransportAdvisor/data/case1/%1").arg(relativeUnderDataCase1)),
    };
    for (const QString &p : candidates) {
        if (QFileInfo::exists(p))
            return QFileInfo(p).absoluteFilePath();
    }
    return QDir::current().filePath(QStringLiteral("data/case1/%1").arg(relativeUnderDataCase1));
}

QString substituteKey(const QString &html, const QString &key)
{
    QString out = html;
    out.replace(QStringLiteral("%%GOOGLE_MAPS_API_KEY%%"), key, Qt::CaseSensitive);
    return out;
}

bool approxStopLatLng(const PtStop &stop, double *latOut, double *lngOut)
{
    const QString n = stop.name.trimmed().toLower();
    if (n.contains(QStringLiteral("central"))) {
        *latOut = 22.2819;
        *lngOut = 114.1582;
        return true;
    }
    if (n.contains(QStringLiteral("admiralty"))) {
        *latOut = 22.2796;
        *lngOut = 114.1659;
        return true;
    }
    if (n.contains(QStringLiteral("wan chai"))) {
        *latOut = 22.2770;
        *lngOut = 114.1757;
        return true;
    }
    if (n.contains(QStringLiteral("causeway bay"))) {
        *latOut = 22.2803;
        *lngOut = 114.1850;
        return true;
    }
    if (n.contains(QStringLiteral("north point"))) {
        *latOut = 22.2919;
        *lngOut = 114.2003;
        return true;
    }
    if (n.contains(QStringLiteral("tsim sha tsui"))) {
        *latOut = 22.2967;
        *lngOut = 114.1722;
        return true;
    }
    if (n.contains(QStringLiteral("mong kok"))) {
        *latOut = 22.3193;
        *lngOut = 114.1700;
        return true;
    }
    if (n.contains(QStringLiteral("tsuen wan"))) {
        *latOut = 22.3730;
        *lngOut = 114.1177;
        return true;
    }
    if (n.contains(QStringLiteral("kennedy town"))) {
        *latOut = 22.2825;
        *lngOut = 114.1280;
        return true;
    }
    if (n.contains(QStringLiteral("hku"))) {
        *latOut = 22.2830;
        *lngOut = 114.1371;
        return true;
    }
    if (n.contains(QStringLiteral("kowloon bay"))) {
        *latOut = 22.3232;
        *lngOut = 114.2147;
        return true;
    }
    if (n.contains(QStringLiteral("diamond hill"))) {
        *latOut = 22.3390;
        *lngOut = 114.2015;
        return true;
    }
    if (n.contains(QStringLiteral("tai wai"))) {
        *latOut = 22.3736;
        *lngOut = 114.1788;
        return true;
    }
    if (n.contains(QStringLiteral("university"))) {
        *latOut = 22.4137;
        *lngOut = 114.2100;
        return true;
    }
    if (n.contains(QStringLiteral("tai po market"))) {
        *latOut = 22.4445;
        *lngOut = 114.1707;
        return true;
    }
    if (n.contains(QStringLiteral("sheung shui"))) {
        *latOut = 22.5011;
        *lngOut = 114.1286;
        return true;
    }
    if (n.contains(QStringLiteral("lo wu"))) {
        *latOut = 22.5286;
        *lngOut = 114.1141;
        return true;
    }
    if (n.contains(QStringLiteral("nam cheong"))) {
        *latOut = 22.3267;
        *lngOut = 114.1531;
        return true;
    }
    if (n.contains(QStringLiteral("austin"))) {
        *latOut = 22.3047;
        *lngOut = 114.1675;
        return true;
    }
    if (n.contains(QStringLiteral("east tsim sha tsui"))) {
        *latOut = 22.2964;
        *lngOut = 114.1748;
        return true;
    }
    if (n.contains(QStringLiteral("hung hom"))) {
        *latOut = 22.3032;
        *lngOut = 114.1821;
        return true;
    }
    if (n.contains(QStringLiteral("hong kong"))) {
        *latOut = 22.2849;
        *lngOut = 114.1589;
        return true;
    }
    // Fallback so newly added stop names still render on map.
    const uint h = qHash(stop.id.isEmpty() ? stop.name : stop.id);
    *latOut = 22.30 + (static_cast<int>(h % 200) - 100) * 0.0012;
    *lngOut = 114.17 + (static_cast<int>((h / 200) % 200) - 100) * 0.0012;
    return true;
}

} // namespace

namespace {

QString readConfigValue(const QString &keyName)
{
    const QByteArray envKey = keyName.toUtf8();
    const QByteArray env = qgetenv(envKey.constData());
    if (!env.isEmpty())
        return QString::fromUtf8(env);

    const QStringList dirs = {QCoreApplication::applicationDirPath(),
                              QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation),
                              QStringLiteral(".")};
    for (const QString &dir : dirs) {
        QFile f(dir + QStringLiteral("/config.ini"));
        if (!f.exists())
            f.setFileName(dir + QStringLiteral("/TransportAdvisor.ini"));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        while (!f.atEnd()) {
            const QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (line.startsWith(QLatin1Char('#')) || line.isEmpty())
                continue;
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq <= 0)
                continue;
            const QString k = line.left(eq).trimmed();
            const QString v = line.mid(eq + 1).trimmed();
            if (k.compare(keyName, Qt::CaseInsensitive) == 0)
                return v;
        }
    }
    return {};
}

} // namespace

QString MainWindow::readApiKey()
{
    return readConfigValue(QStringLiteral("GOOGLE_MAPS_API_KEY"));
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Smart Public Transport Advisor"));
    resize(1280, 800);

    m_routes = new RouteService(this);
    m_routes->setApiKey(readApiKey());
    m_bridge = new TransportBridge(m_routes, this);
    const QString csvDir = readConfigValue(QStringLiteral("DATA_CSV_DIR"));
    if (!csvDir.isEmpty())
        m_bridge->setLocalCsvDirectory(csvDir);

    m_view = new QWebEngineView(this);
    setCentralWidget(m_view);

    auto *channel = new QWebChannel(this);
    channel->registerObject(QStringLiteral("transport"), m_bridge);
    m_view->page()->setWebChannel(channel);
    connect(m_view, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (!ok)
            return;
        // Re-sync after web map initialization to avoid losing overlays due to startup timing.
        QTimer::singleShot(300, this, [this]() { syncCourseworkNetworkOverlayOnMap(); });
    });

    connect(m_bridge, &TransportBridge::searchFailed, this, [this](const QString &msg) {
        QMessageBox::warning(this, QStringLiteral("Advisor"), msg);
    });

    loadMapHtml();
    // Keep networkModel graph-planning flow active in the desktop app.
    setupCourseworkGraphDock();
}

void MainWindow::loadMapHtml()
{
    const QString apiKey = readApiKey();
    m_routes->setApiKey(apiKey);

    QFile f(QStringLiteral(":/html/map.html"));
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, QStringLiteral("Advisor"), QStringLiteral("Missing embedded map.html."));
        return;
    }
    QString html = QString::fromUtf8(f.readAll());
    html = substituteKey(html, apiKey);
    m_view->setHtml(html, QUrl(QStringLiteral("qrc:/html/")));
}

void MainWindow::setupCourseworkGraphDock()
{
    auto *dock = new QDockWidget(tr("Graph journey planner"), this);
    dock->setObjectName(QStringLiteral("CourseworkGraphDock"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto *panel = new QWidget(dock);
    auto *lay = new QVBoxLayout(panel);

    m_ptLoadStatus = new QLabel(tr("Loading coursework graph…"), panel);
    m_ptLoadStatus->setWordWrap(true);
    lay->addWidget(m_ptLoadStatus);

    lay->addWidget(new QLabel(tr("Network dataset:"), panel));
    m_ptNetworkCombo = new QComboBox(panel);
    m_ptNetworkCombo->addItem(tr("Default case1 (data/case1)"),
                              QStringLiteral("%1|%2")
                                  .arg(resolveCourseworkDataFile(QStringLiteral("stops.csv")),
                                       resolveCourseworkDataFile(QStringLiteral("segments.csv"))));
    m_ptNetworkCombo->addItem(tr("networkModel map 01 (10 stops)"),
                              QStringLiteral("%1|%2")
                                  .arg(QDir::current().filePath(QStringLiteral("networkModel/map/stop01.txt")),
                                       QDir::current().filePath(QStringLiteral("networkModel/map/seg01.txt"))));
    m_ptNetworkCombo->addItem(tr("networkModel map 02"),
                              QStringLiteral("%1|%2")
                                  .arg(QDir::current().filePath(QStringLiteral("networkModel/map/stop02.txt")),
                                       QDir::current().filePath(QStringLiteral("networkModel/map/seg02.txt"))));
    m_ptNetworkCombo->addItem(tr("networkModel map 07"),
                              QStringLiteral("%1|%2")
                                  .arg(QDir::current().filePath(QStringLiteral("networkModel/map/stop07.txt")),
                                       QDir::current().filePath(QStringLiteral("networkModel/map/seg07.txt"))));
    lay->addWidget(m_ptNetworkCombo);

    auto *loadNetworkBtn = new QPushButton(tr("Load selected network"), panel);
    lay->addWidget(loadNetworkBtn);

    lay->addWidget(new QLabel(tr("Origin stop:"), panel));
    m_ptOriginCombo = new QComboBox(panel);
    lay->addWidget(m_ptOriginCombo);

    lay->addWidget(new QLabel(tr("Destination stop:"), panel));
    m_ptDestCombo = new QComboBox(panel);
    lay->addWidget(m_ptDestCombo);

    lay->addWidget(new QLabel(tr("Preference:"), panel));
    m_ptPrefCombo = new QComboBox(panel);
    m_ptPrefCombo->addItem(tr("Fastest"), QStringLiteral("fastest"));
    m_ptPrefCombo->addItem(tr("Cheapest"), QStringLiteral("cheapest"));
    m_ptPrefCombo->addItem(tr("Fewest segments"), QStringLiteral("fewest_segments"));
    m_ptPrefCombo->addItem(tr("Fewest transfers"), QStringLiteral("fewest_transfers"));
    lay->addWidget(m_ptPrefCombo);

    auto *findBtn = new QPushButton(tr("Find top journeys"), panel);
    lay->addWidget(findBtn);

    lay->addWidget(new QLabel(tr("Top routes (click to draw on map):"), panel));
    for (int i = 0; i < 3; ++i) {
        m_ptRouteBtn[i] = new QPushButton(tr("Route %1").arg(i + 1), panel);
        m_ptRouteBtn[i]->setEnabled(false);
        lay->addWidget(m_ptRouteBtn[i]);
        connect(m_ptRouteBtn[i], &QPushButton::clicked, this, [this, i]() { onCourseworkRoutePicked(i); });
    }

    m_ptResults = new QTextEdit(panel);
    m_ptResults->setReadOnly(true);
    m_ptResults->setMinimumHeight(220);
    lay->addWidget(m_ptResults, 1);

    dock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    connect(findBtn, &QPushButton::clicked, this, &MainWindow::onCourseworkGraphFind);
    connect(loadNetworkBtn, &QPushButton::clicked, this, &MainWindow::onCourseworkGraphLoadSelectedNetwork);

    reloadCourseworkGraphFromDefaultFiles();
}

void MainWindow::reloadCourseworkGraphFromDefaultFiles()
{
    const QString stopsPath = resolveCourseworkDataFile(QStringLiteral("stops.csv"));
    const QString segsPath = resolveCourseworkDataFile(QStringLiteral("segments.csv"));
    QString err;
    if (!m_ptGraph.loadFromFiles(stopsPath, segsPath, &err)) {
        m_ptLoadStatus->setText(tr("Coursework graph not loaded.\n%1\n\nPaths tried include:\n%2\n%3")
                                     .arg(err, stopsPath, segsPath));
        m_ptOriginCombo->clear();
        m_ptDestCombo->clear();
        m_ptLastTopJourneys.clear();
        m_ptLastOrigin.clear();
        for (int i = 0; i < 3; ++i)
            setRouteButtonState(i, false, tr("Route %1").arg(i + 1));
        m_ptResults->clear();
        return;
    }
    m_ptLoadStatus->setText(tr("%1 stops, %2 directed segments\n%3 / %4")
                                .arg(m_ptGraph.stops().size())
                                .arg(m_ptGraph.directedSegmentCount())
                                .arg(QFileInfo(stopsPath).fileName(), QFileInfo(segsPath).fileName()));
    fillCourseworkGraphStopCombos();
    syncCourseworkNetworkOverlayOnMap();
    m_ptLastTopJourneys.clear();
    m_ptLastOrigin.clear();
    for (int i = 0; i < 3; ++i)
        setRouteButtonState(i, false, tr("Route %1").arg(i + 1));
    m_ptResults->setPlainText(m_ptGraph.summaryText());
}

void MainWindow::onCourseworkGraphLoadSelectedNetwork()
{
    if (m_ptNetworkCombo == nullptr)
        return;
    const QString pair = m_ptNetworkCombo->currentData().toString();
    const QStringList parts = pair.split(QLatin1Char('|'));
    if (parts.size() != 2) {
        QMessageBox::warning(this, tr("Graph journey planner"), tr("Invalid network dataset path."));
        return;
    }
    const QString stopsPath = QFileInfo(parts.at(0)).absoluteFilePath();
    const QString segsPath = QFileInfo(parts.at(1)).absoluteFilePath();

    QString err;
    if (!m_ptGraph.loadFromFiles(stopsPath, segsPath, &err)) {
        QMessageBox::warning(this, tr("Graph journey planner"), tr("Load failed:\n%1").arg(err));
        return;
    }
    m_ptLoadStatus->setText(tr("%1 stops, %2 directed segments\n%3 / %4")
                                .arg(m_ptGraph.stops().size())
                                .arg(m_ptGraph.directedSegmentCount())
                                .arg(QFileInfo(stopsPath).fileName(), QFileInfo(segsPath).fileName()));
    fillCourseworkGraphStopCombos();
    syncCourseworkNetworkOverlayOnMap();
    m_ptLastTopJourneys.clear();
    m_ptLastOrigin.clear();
    for (int i = 0; i < 3; ++i)
        setRouteButtonState(i, false, tr("Route %1").arg(i + 1));
    m_ptResults->setPlainText(m_ptGraph.summaryText());
}

void MainWindow::syncCourseworkNetworkOverlayOnMap()
{
    if (m_view == nullptr || m_view->page() == nullptr)
        return;

    QJsonArray nodes;
    for (auto it = m_ptGraph.stops().constBegin(); it != m_ptGraph.stops().constEnd(); ++it) {
        const QString stopId = it.key();
        const PtStop stop = it.value();
        double lat = 0.0;
        double lng = 0.0;
        if (!approxStopLatLng(stop, &lat, &lng))
            continue;
        QJsonObject node;
        node.insert(QStringLiteral("id"), stopId);
        node.insert(QStringLiteral("name"), stop.name);
        node.insert(QStringLiteral("type"), stop.stopType);
        node.insert(QStringLiteral("lat"), lat);
        node.insert(QStringLiteral("lng"), lng);
        nodes.append(node);
    }

    QJsonArray segments;
    for (auto it = m_ptGraph.adjacency().constBegin(); it != m_ptGraph.adjacency().constEnd(); ++it) {
        const QString fromId = it.key();
        const PtStop fromStop = m_ptGraph.stops().value(fromId);
        double fromLat = 0.0;
        double fromLng = 0.0;
        if (!approxStopLatLng(fromStop, &fromLat, &fromLng))
            continue;
        for (const PtEdge &edge : it.value()) {
            const PtStop toStop = m_ptGraph.stops().value(edge.toId);
            double toLat = 0.0;
            double toLng = 0.0;
            if (!approxStopLatLng(toStop, &toLat, &toLng))
                continue;
            QJsonObject seg;
            QJsonArray a;
            a.append(fromLat);
            a.append(fromLng);
            QJsonArray b;
            b.append(toLat);
            b.append(toLng);
            seg.insert(QStringLiteral("from"), a);
            seg.insert(QStringLiteral("to"), b);
            seg.insert(QStringLiteral("mode"), edge.mode);
            segments.append(seg);
        }
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("nodes"), nodes);
    payload.insert(QStringLiteral("segments"), segments);
    const QString js = QStringLiteral("window.showCourseworkNetworkOverlay(%1);")
                           .arg(QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
    m_view->page()->runJavaScript(js);
}

void MainWindow::fillCourseworkGraphStopCombos()
{
    m_ptOriginCombo->clear();
    m_ptDestCombo->clear();
    QStringList ids = m_ptGraph.stops().keys();
    std::sort(ids.begin(), ids.end());
    for (const QString &sid : ids) {
        const PtStop st = m_ptGraph.stops().value(sid);
        const QString label = QStringLiteral("%1 — %2").arg(sid, st.name);
        m_ptOriginCombo->addItem(label, sid);
        m_ptDestCombo->addItem(label, sid);
    }
}

void MainWindow::onCourseworkGraphFind()
{
    if (m_ptGraph.stops().isEmpty()) {
        QMessageBox::information(this, tr("Graph journey planner"),
                                 tr("No coursework graph loaded. Place data/case1/stops.csv and segments.csv where the "
                                    "app can find them, then restart."));
        return;
    }

    const QString origin = m_ptOriginCombo->currentData().toString().trimmed();
    const QString dest = m_ptDestCombo->currentData().toString().trimmed();
    if (origin.isEmpty() || dest.isEmpty()) {
        QMessageBox::warning(this, tr("Graph journey planner"), tr("Choose origin and destination stops."));
        return;
    }
    if (origin == dest) {
        QMessageBox::warning(this, tr("Graph journey planner"), tr("Origin and destination must differ."));
        return;
    }

    bool ok = false;
    const PtPreference pref = parsePreference(m_ptPrefCombo->currentData().toString(), &ok);
    const PtPreference usePref = ok ? pref : PtPreference::Fastest;

    QVector<PtJourney> paths = m_ptGraph.findJourneys(origin, dest, 8);
    if (paths.isEmpty()) {
        m_ptLastTopJourneys.clear();
        m_ptLastOrigin.clear();
        for (int i = 0; i < 3; ++i)
            setRouteButtonState(i, false, tr("Route %1").arg(i + 1));
        m_ptResults->setPlainText(tr("No journey found between the selected stops."));
        return;
    }

    PtGraphAdvisor::rankJourneys(paths, usePref);

    m_ptLastOrigin = origin;
    m_ptLastTopJourneys.clear();
    QString out;
    const int n = qMin(3, paths.size());
    for (int i = 0; i < n; ++i) {
        const PtJourney journey = paths.at(i);
        const QString routeOrigin = origin;
        m_ptLastTopJourneys.append(journey);

        const QStringList seq = journey.stopSequence(routeOrigin);
        const QString shortRoute = seq.size() >= 2 ? QStringLiteral("%1 -> %2").arg(seq.first(), seq.last()) : seq.join(QStringLiteral(" -> "));
        setRouteButtonState(i, true, tr("Route #%1 - %2 min - HKD %3 (%4)")
                                      .arg(i + 1)
                                      .arg(journey.totalDuration())
                                      .arg(journey.totalCost(), 0, 'f', 2)
                                      .arg(shortRoute));

        if (!out.isEmpty())
            out += QStringLiteral("\n----------------------------------------\n\n");
        out += QStringLiteral("Route #%1\n").arg(i + 1);
        out += PtGraphAdvisor::formatJourneyReport(journey, routeOrigin, m_ptGraph.stops()).trimmed();
        out += QLatin1Char('\n');
    }
    for (int i = n; i < 3; ++i)
        setRouteButtonState(i, false, tr("Route %1").arg(i + 1));
    m_ptResults->setPlainText(out.trimmed());

    if (!m_ptLastTopJourneys.isEmpty())
        showCourseworkJourneyOnMap(m_ptLastTopJourneys.first(), m_ptLastOrigin);
}

void MainWindow::onCourseworkRoutePicked(int index)
{
    if (index < 0 || index >= m_ptLastTopJourneys.size())
        return;
    showCourseworkJourneyOnMap(m_ptLastTopJourneys.at(index), m_ptLastOrigin);
}

void MainWindow::setRouteButtonState(int index, bool enabled, const QString &text)
{
    if (index < 0 || index >= 3 || m_ptRouteBtn[index] == nullptr)
        return;
    m_ptRouteBtn[index]->setEnabled(enabled);
    m_ptRouteBtn[index]->setText(text);
}

void MainWindow::showCourseworkJourneyOnMap(const PtJourney &journey, const QString &originId)
{
    if (m_view == nullptr || m_view->page() == nullptr)
        return;

    QJsonArray latLngs;
    const QStringList seq = journey.stopSequence(originId);
    for (const QString &stopId : seq) {
        const PtStop stop = m_ptGraph.stops().value(stopId);
        double lat = 0.0;
        double lng = 0.0;
        if (!approxStopLatLng(stop, &lat, &lng))
            continue;
        QJsonArray pair;
        pair.append(lat);
        pair.append(lng);
        latLngs.append(pair);
    }
    if (latLngs.size() < 2) {
        QMessageBox::information(this, tr("Graph journey planner"),
                                 tr("Could not map this route to coordinates for drawing."));
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("path"), latLngs);
    payload.insert(QStringLiteral("label"), seq.join(QStringLiteral(" -> ")));
    const QString js = QStringLiteral("window.highlightCourseworkRoute(%1);")
                           .arg(QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
    m_view->page()->runJavaScript(js);
}
