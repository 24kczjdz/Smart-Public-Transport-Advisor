#include "PtGraphAdvisor.h"
#include "PtGraphTypes.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <iostream>
#include <limits>

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

static void pauseConsole()
{
    std::cout << "\nPress Enter to continue...";
    std::cout.flush();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
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

    QString stopsPath;
    QString segsPath;
    if (argc >= 3) {
        stopsPath = QString::fromLocal8Bit(argv[1]);
        segsPath = QString::fromLocal8Bit(argv[2]);
    } else {
        stopsPath = resolveDefaultDataFile(QStringLiteral("stops.csv"));
        segsPath = resolveDefaultDataFile(QStringLiteral("segments.csv"));
    }

    PtGraphAdvisor graph;
    QString err;
    if (!graph.loadFromFiles(stopsPath, segsPath, &err)) {
        std::cerr << "Load error: " << err.toStdString() << "\n";
        std::cerr << "Usage: pt_advisor [stops.csv segments.csv]\n";
        std::cerr << "Tried default: " << stopsPath.toStdString() << " / " << segsPath.toStdString() << "\n";
        return 1;
    }

    std::cout << "Loaded: " << graph.stops().size() << " stops, " << graph.directedSegmentCount()
              << " directed segments\n";
    std::cout << "Stops file: " << stopsPath.toStdString() << "\n";
    std::cout << "Segments file: " << segsPath.toStdString() << "\n";

    for (;;) {
        std::cout << "\n========================================\n";
        std::cout << "1. List all stops\n";
        std::cout << "2. Query journeys\n";
        std::cout << "3. Network summary\n";
        std::cout << "4. Exit\n";
        std::cout << "Choose (1-4): ";
        std::cout.flush();
        std::string lineIn;
        if (!std::getline(std::cin, lineIn))
            break;
        const QString choice = QString::fromStdString(lineIn).trimmed();

        if (choice == QLatin1String("1")) {
            std::cout << "\nStop list:\n";
            QStringList ids = graph.stops().keys();
            std::sort(ids.begin(), ids.end());
            for (const QString &sid : ids) {
                const PtStop st = graph.stops().value(sid);
                std::cout << "  " << sid.toStdString() << ": " << st.name.toStdString() << " (" << st.stopType.toStdString()
                          << ")\n";
            }
        } else if (choice == QLatin1String("2")) {
            QStringList ids = graph.stops().keys();
            std::sort(ids.begin(), ids.end());
            std::cout << "\nAvailable stop IDs: " << ids.join(QLatin1String(", ")).toStdString() << "\n";
            std::cout << "Origin stop ID: ";
            std::cout.flush();
            std::string o, d;
            std::getline(std::cin, o);
            std::cout << "Destination stop ID: ";
            std::cout.flush();
            std::getline(std::cin, d);
            const QString origin = QString::fromStdString(o).trimmed();
            const QString dest = QString::fromStdString(d).trimmed();
            if (!graph.stops().contains(origin) || !graph.stops().contains(dest)) {
                std::cout << "Error: stop does not exist\n";
            } else if (origin == dest) {
                std::cout << "Error: origin and destination are the same\n";
            } else {
                std::cout << "Preference (cheapest/fastest/fewest_segments/fewest_transfers): ";
                std::cout.flush();
                std::string prefStr;
                std::getline(std::cin, prefStr);
                bool ok = false;
                PtPreference pref = parsePreference(QString::fromStdString(prefStr), &ok);
                if (!ok) {
                    pref = PtPreference::Fastest;
                    std::cout << "Invalid preference, using 'fastest'\n";
                }
                printTopJourneys(graph, origin, dest, pref, 3);
            }
        } else if (choice == QLatin1String("3")) {
            std::cout << "\n" << graph.summaryText().toStdString();
        } else if (choice == QLatin1String("4")) {
            std::cout << "Goodbye!\n";
            break;
        } else {
            std::cout << "Invalid choice\n";
        }
        pauseConsole();
    }
    return 0;
}
