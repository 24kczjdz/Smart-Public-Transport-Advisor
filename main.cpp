#include "MainWindow.h"
#include "PtGraphAdvisor.h"
#include "PtGraphTypes.h"

#include <QApplication>
#include <QCoreApplication>
#include <QStringList>

#include <algorithm>
#include <iostream>

namespace {

struct Nv02CliOptions {
    QString stopsPath = QStringLiteral("stops.txt");
    QString segmentsPath = QStringLiteral("segments.txt");
    QString origin;
    QString dest;
    QString pref = QStringLiteral("fastest");
    int maxPaths = 200;
    int maxLen = 8;
};

void printTopJourneys(PtGraphAdvisor &graph, const QString &origin, const QString &dest, PtPreference pref, int topK, int maxLen,
                      int maxPaths)
{
    QVector<PtJourney> paths = graph.findJourneys(origin, dest, maxLen);
    if (paths.isEmpty()) {
        std::cout << "No journey found.\n";
        return;
    }
    PtGraphAdvisor::rankJourneys(paths, pref);
    if (maxPaths > 0 && paths.size() > maxPaths)
        paths.resize(maxPaths);

    const int n = qMin(topK, paths.size());
    for (int i = 0; i < n; ++i) {
        std::cout << "\n#" << (i + 1) << "\n";
        std::cout << PtGraphAdvisor::formatJourneyReport(paths.at(i), origin, graph.stops()).toStdString();
    }
    std::cout << "\n";
}

bool parseNv02StyleArgs(int argc, char *argv[], Nv02CliOptions *out)
{
    bool hasNvStyle = false;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a.startsWith(QStringLiteral("--"))) {
            hasNvStyle = true;
            break;
        }
    }
    if (!hasNvStyle)
        return false;

    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        auto needVal = [&](QString *target) {
            if (i + 1 < argc) {
                *target = QString::fromLocal8Bit(argv[++i]).trimmed();
                return true;
            }
            return false;
        };
        if (a == QStringLiteral("--stops")) {
            if (!needVal(&out->stopsPath))
                return false;
        } else if (a == QStringLiteral("--segments")) {
            if (!needVal(&out->segmentsPath))
                return false;
        } else if (a == QStringLiteral("--origin")) {
            if (!needVal(&out->origin))
                return false;
        } else if (a == QStringLiteral("--dest")) {
            if (!needVal(&out->dest))
                return false;
        } else if (a == QStringLiteral("--preference")) {
            if (!needVal(&out->pref))
                return false;
        } else if (a == QStringLiteral("--max-paths")) {
            QString tmp;
            if (!needVal(&tmp))
                return false;
            out->maxPaths = tmp.toInt();
        } else if (a == QStringLiteral("--max-len")) {
            QString tmp;
            if (!needVal(&tmp))
                return false;
            out->maxLen = tmp.toInt();
        }
    }
    return true;
}

int runNv02Query(const Nv02CliOptions &opts)
{
    PtGraphAdvisor graph;
    QString err;
    if (!graph.loadFromFiles(opts.stopsPath, opts.segmentsPath, &err)) {
        std::cerr << "Error: " << err.toStdString() << "\n";
        return 1;
    }
    if (!graph.stops().contains(opts.origin) || !graph.stops().contains(opts.dest)) {
        std::cerr << "Error: unknown stop id (origin or destination).\n";
        return 1;
    }
    if (opts.origin == opts.dest) {
        std::cerr << "Error: origin and destination are the same.\n";
        return 1;
    }

    bool ok = false;
    PtPreference pref = parsePreference(opts.pref, &ok);
    if (!ok) {
        pref = PtPreference::Fastest;
        std::cerr << "Invalid preference, using 'fastest'\n";
    }

    std::cout << "Loaded: " << graph.stops().size() << " stops, " << graph.directedSegmentCount() << " directed segments\n";
    printTopJourneys(graph, opts.origin, opts.dest, pref, 3, qMax(1, opts.maxLen), qMax(1, opts.maxPaths));
    return 0;
}

int runNv02Interactive()
{
    QString stopsPath = QStringLiteral("stops.txt");
    QString segsPath = QStringLiteral("segments.txt");
    PtGraphAdvisor graph;
    QString err;
    if (!graph.loadFromFiles(stopsPath, segsPath, &err)) {
        stopsPath = QStringLiteral("networkModel/map/stop01.txt");
        segsPath = QStringLiteral("networkModel/map/seg01.txt");
        if (graph.loadFromFiles(stopsPath, segsPath, &err))
            std::cout << "Loaded default networkModel map01.\n";
        else
            std::cout << "No default network found. Use option 4 to load a network file.\n";
    } else {
        std::cout << "Loaded default network: " << graph.stops().size() << " stops, " << graph.directedSegmentCount()
                  << " directed segments\n";
    }

    while (true) {
        std::cout << "\n========================================\n\n";
        std::cout << "1. List all stops\n2. Query journeys\n3. Network summary\n4. Load new network\n5. Exit\n";
        std::cout << "Choose (1-5): ";
        std::string choice;
        std::getline(std::cin, choice);

        if ((choice == "1" || choice == "2" || choice == "3") && graph.stops().isEmpty()) {
            std::cout << "No network loaded. Please use option 4 first.\n";
        } else if (choice == "1") {
            QStringList ids = graph.stops().keys();
            std::sort(ids.begin(), ids.end());
            std::cout << "\nStop list:\n";
            for (const QString &sid : ids) {
                const PtStop st = graph.stops().value(sid);
                std::cout << "  " << sid.toStdString() << ": " << st.name.toStdString() << " (" << st.stopType.toStdString()
                          << ")\n";
            }
        } else if (choice == "2") {
            std::cout << "\nOrigin stop ID: ";
            std::string o;
            std::getline(std::cin, o);
            std::cout << "Destination stop ID: ";
            std::string d;
            std::getline(std::cin, d);
            const QString origin = QString::fromStdString(o).trimmed();
            const QString dest = QString::fromStdString(d).trimmed();
            if (!graph.stops().contains(origin) || !graph.stops().contains(dest)) {
                std::cout << "Error: stop does not exist\n";
            } else if (origin == dest) {
                std::cout << "Error: origin and destination are the same\n";
            } else {
                std::cout << "Preference (cheapest/fastest/fewest_segments/fewest_transfers): ";
                std::string p;
                std::getline(std::cin, p);
                bool ok = false;
                PtPreference pref = parsePreference(QString::fromStdString(p).trimmed(), &ok);
                if (!ok) {
                    pref = PtPreference::Fastest;
                    std::cout << "Invalid preference, using 'fastest'\n";
                }
                printTopJourneys(graph, origin, dest, pref, 3, 8, 200);
            }
        } else if (choice == "3") {
            std::cout << "\n" << graph.summaryText().toStdString();
        } else if (choice == "4") {
            std::cout << "Stops file path: ";
            std::string sp;
            std::getline(std::cin, sp);
            std::cout << "Segments file path: ";
            std::string gp;
            std::getline(std::cin, gp);
            PtGraphAdvisor tmp;
            QString loadErr;
            if (!tmp.loadFromFiles(QString::fromStdString(sp).trimmed(), QString::fromStdString(gp).trimmed(), &loadErr)) {
                std::cout << "Error: " << loadErr.toStdString() << ". Network unchanged.\n";
            } else {
                graph = tmp;
                std::cout << "Loaded: " << graph.stops().size() << " stops, " << graph.directedSegmentCount()
                          << " directed segments\n";
            }
        } else if (choice == "5") {
            std::cout << "Goodbye!\n";
            break;
        } else {
            std::cout << "Invalid choice\n";
        }
        std::cout << "\nPress Enter to continue...";
        std::string dummy;
        std::getline(std::cin, dummy);
    }
    return 0;
}

int runCliMode(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("TransportAdvisor CLI"));

    Nv02CliOptions opts;
    if (parseNv02StyleArgs(argc, argv, &opts)) {
        if (!opts.origin.isEmpty() && !opts.dest.isEmpty())
            return runNv02Query(opts);
        return runNv02Interactive();
    }

    if (argc < 5) {
        std::cerr << "Usage: TransportAdvisor <stops.csv> <segments.csv> <origin_stop_id> <dest_stop_id> [preference]\n"
                     "  or:  TransportAdvisor --stops <path> --segments <path> [--origin <id> --dest <id>]\n";
        return 1;
    }

    opts.stopsPath = QString::fromLocal8Bit(argv[1]);
    opts.segmentsPath = QString::fromLocal8Bit(argv[2]);
    opts.origin = QString::fromLocal8Bit(argv[3]).trimmed();
    opts.dest = QString::fromLocal8Bit(argv[4]).trimmed();
    if (argc >= 6)
        opts.pref = QString::fromLocal8Bit(argv[5]).trimmed();
    return runNv02Query(opts);
}

} // namespace

int main(int argc, char *argv[])
{
    // If CSV/source-destination args are supplied, run CLI planner flow.
    if (argc >= 5)
        return runCliMode(argc, argv);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("TransportAdvisor"));
    QApplication::setOrganizationDomain(QStringLiteral("local"));
    MainWindow w;
    w.show();
    return app.exec();
}
