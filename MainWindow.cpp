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
#include <QIODevice>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextEdit>
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

    connect(m_bridge, &TransportBridge::searchFailed, this, [this](const QString &msg) {
        QMessageBox::warning(this, QStringLiteral("Advisor"), msg);
    });

    loadMapHtml();
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

    m_ptResults = new QTextEdit(panel);
    m_ptResults->setReadOnly(true);
    m_ptResults->setMinimumHeight(220);
    lay->addWidget(m_ptResults, 1);

    dock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    connect(findBtn, &QPushButton::clicked, this, &MainWindow::onCourseworkGraphFind);

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
        m_ptResults->clear();
        return;
    }
    m_ptLoadStatus->setText(tr("%1 stops, %2 directed segments\n%3 / %4")
                                .arg(m_ptGraph.stops().size())
                                .arg(m_ptGraph.directedSegmentCount())
                                .arg(QFileInfo(stopsPath).fileName(), QFileInfo(segsPath).fileName()));
    fillCourseworkGraphStopCombos();
    m_ptResults->setPlainText(m_ptGraph.summaryText());
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
        m_ptResults->setPlainText(tr("No journey found between the selected stops."));
        return;
    }
    PtGraphAdvisor::rankJourneys(paths, usePref);

    QString out;
    const int n = qMin(3, paths.size());
    for (int i = 0; i < n; ++i) {
        out += QStringLiteral("#%1\n").arg(i + 1);
        out += PtGraphAdvisor::formatJourneyReport(paths.at(i), origin, m_ptGraph.stops());
        out += QLatin1Char('\n');
    }
    m_ptResults->setPlainText(out.trimmed());
}
