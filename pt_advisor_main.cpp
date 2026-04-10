#include "PtGraphAdvisor.h"
#include "PtGraphTypes.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <iostream>

static QString resolveDefaultDataFile(const QString &relativeUnderDataCase1)
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

static void printTopJourneys(PtGraphAdvisor &g, const QString &origin, const QString &dest, PtPreference pref, int topK)
{
    QVector<PtJourney> paths = g.findJourneys(origin, dest, 8);
    if (paths.isEmpty()) {
        std::cout << "No journey found.\n";
        return;
    }
    PtGraphAdvisor::rankJourneys(paths, pref);
    const int n = qMin(topK, paths.size());
    const char *prefName = "fastest";
    switch (pref) {
    case PtPreference::Cheapest:
        prefName = "cheapest";
        break;
    case PtPreference::FewestSegments:
        prefName = "fewest_segments";
        break;
    case PtPreference::FewestTransfers:
        prefName = "fewest_transfers";
        break;
    default:
        break;
    }
    std::cout << "\n=== Top " << n << " journeys (preference: " << prefName << ") ===\n";
    for (int i = 0; i < n; ++i) {
        std::cout << "\n#" << (i + 1) << "\n";
        const QString txt = PtGraphAdvisor::formatJourneyReport(paths.at(i), origin, g.stops());
        std::cout << txt.toStdString();
    }
    std::cout << "\n";
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("pt_advisor"));

    if (argc < 5) {
        std::cerr << "Usage: pt_advisor <stops.csv> <segments.csv> <origin_stop_id> <dest_stop_id> [preference]\n"
                     "  preference: cheapest | fastest | fewest_segments | fewest_transfers (default: fastest)\n"
                     "\n"
                     "For interactive input, use the Smart Public Transport Advisor desktop app:\n"
                     "  open the Graph journey planner dock (origin, destination, preference, Find).\n"
                     "\n"
                     "Example:\n"
                     "  pt_advisor data/case1/stops.csv data/case1/segments.csv CEN_MTR WCH_MTR cheapest\n";
        return 1;
    }

    const QString stopsPath = QString::fromLocal8Bit(argv[1]);
    const QString segsPath = QString::fromLocal8Bit(argv[2]);
    const QString origin = QString::fromLocal8Bit(argv[3]).trimmed();
    const QString dest = QString::fromLocal8Bit(argv[4]).trimmed();
    QString prefStr = QStringLiteral("fastest");
    if (argc >= 6)
        prefStr = QString::fromLocal8Bit(argv[5]).trimmed();

    PtGraphAdvisor graph;
    QString err;
    if (!graph.loadFromFiles(stopsPath, segsPath, &err)) {
        std::cerr << "Load error: " << err.toStdString() << "\n";
        return 1;
    }

    if (!graph.stops().contains(origin) || !graph.stops().contains(dest)) {
        std::cerr << "Error: unknown stop id (origin or destination).\n";
        return 1;
    }
    if (origin == dest) {
        std::cerr << "Error: origin and destination are the same.\n";
        return 1;
    }

    bool ok = false;
    PtPreference pref = parsePreference(prefStr, &ok);
    if (!ok) {
        pref = PtPreference::Fastest;
        std::cerr << "Invalid preference, using 'fastest'\n";
    }

    std::cout << "Loaded: " << graph.stops().size() << " stops, " << graph.directedSegmentCount()
              << " directed segments\n";
    printTopJourneys(graph, origin, dest, pref, 3);
    return 0;
}
