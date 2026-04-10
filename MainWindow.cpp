#include "MainWindow.h"

#include "RouteService.h"
#include "TransportBridge.h"

#include <QCoreApplication>
#include <QFile>
#include <QIODevice>
#include <QMessageBox>
#include <QStandardPaths>
#include <QWebChannel>
#include <QWebEngineView>

namespace {

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
