#include "PtGraphAdvisor.h"

#include <QFile>
#include <QSet>
#include <QTextStream>
#include <algorithm>
#include <functional>
#include <tuple>

namespace {

bool isCommentOrEmpty(const QString &line)
{
    const QString t = line.trimmed();
    return t.isEmpty() || t.startsWith(QLatin1Char('#'));
}

} // namespace

QStringList PtGraphAdvisor::splitCsvLine(const QString &line)
{
    QStringList fields;
    QString cur;
    bool inQuotes = false;
    for (int i = 0; i < line.length(); ++i) {
        const QChar c = line.at(i);
        if (c == QLatin1Char('"')) {
            inQuotes = !inQuotes;
            continue;
        }
        if (!inQuotes && c == QLatin1Char(',')) {
            fields.append(cur);
            cur.clear();
            continue;
        }
        cur.append(c);
    }
    fields.append(cur);
    for (QString &f : fields)
        f = f.trimmed();
    return fields;
}

bool PtGraphAdvisor::parseStopsHeader(const QStringList &header, int *idCol, int *nameCol, int *typeCol, int *regionCol)
{
    *idCol = *nameCol = *typeCol = -1;
    *regionCol = -1;
    for (int i = 0; i < header.size(); ++i) {
        const QString h = header.at(i).toLower();
        if (h == QLatin1String("stop_id") || h == QLatin1String("id"))
            *idCol = i;
        else if (h == QLatin1String("stop_name") || h == QLatin1String("name"))
            *nameCol = i;
        else if (h == QLatin1String("type"))
            *typeCol = i;
        else if (h == QLatin1String("region") || h == QLatin1String("zone"))
            *regionCol = i;
    }
    return *idCol >= 0 && *nameCol >= 0 && *typeCol >= 0;
}

bool PtGraphAdvisor::parseSegmentsHeader(const QStringList &header, int *fromCol, int *toCol, int *durCol, int *costCol,
                                         int *modeCol)
{
    *fromCol = *toCol = *durCol = *costCol = *modeCol = -1;
    for (int i = 0; i < header.size(); ++i) {
        const QString h = header.at(i).toLower();
        if (h == QLatin1String("from") || h == QLatin1String("from_stop"))
            *fromCol = i;
        else if (h == QLatin1String("to") || h == QLatin1String("to_stop"))
            *toCol = i;
        else if (h == QLatin1String("duration"))
            *durCol = i;
        else if (h == QLatin1String("cost"))
            *costCol = i;
        else if (h == QLatin1String("mode"))
            *modeCol = i;
    }
    return *fromCol >= 0 && *toCol >= 0 && *durCol >= 0 && *costCol >= 0 && *modeCol >= 0;
}

bool PtGraphAdvisor::loadFromFiles(const QString &stopsCsvPath, const QString &segmentsCsvPath, QString *errorOut)
{
    m_stops.clear();
    m_adj.clear();

    QFile sf(stopsCsvPath);
    if (!sf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = QStringLiteral("Cannot open stops file: %1").arg(stopsCsvPath);
        return false;
    }
    QTextStream stIn(&sf);
    const QString headerLine = stIn.readLine();
    if (headerLine.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Stops file is empty");
        return false;
    }
    QStringList hdr = splitCsvLine(headerLine);
    int ic = -1, nc = -1, tc = -1, rc = -1;
    if (!parseStopsHeader(hdr, &ic, &nc, &tc, &rc)) {
        if (errorOut)
            *errorOut = QStringLiteral("Stops CSV: need columns stop_id, stop_name, type (optional: region)");
        return false;
    }
    while (!stIn.atEnd()) {
        const QString line = stIn.readLine();
        if (isCommentOrEmpty(line))
            continue;
        const QStringList p = splitCsvLine(line);
        if (p.size() <= qMax(qMax(ic, nc), tc))
            continue;
        PtStop s;
        s.id = p.at(ic);
        s.name = p.at(nc);
        s.stopType = p.at(tc);
        s.region = rc >= 0 && rc < p.size() ? p.at(rc) : s.stopType;
        m_stops.insert(s.id, s);
    }
    sf.close();

    QFile gf(segmentsCsvPath);
    if (!gf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = QStringLiteral("Cannot open segments file: %1").arg(segmentsCsvPath);
        return false;
    }
    QTextStream gIn(&gf);
    const QString gHeaderLine = gIn.readLine();
    hdr = splitCsvLine(gHeaderLine);
    int fc = -1, toc = -1, dc = -1, cc = -1, mc = -1;
    if (!parseSegmentsHeader(hdr, &fc, &toc, &dc, &cc, &mc)) {
        if (errorOut)
            *errorOut = QStringLiteral("Segments CSV: need from, to, duration, cost, mode");
        return false;
    }
    while (!gIn.atEnd()) {
        const QString line = gIn.readLine();
        if (isCommentOrEmpty(line))
            continue;
        const QStringList p = splitCsvLine(line);
        if (p.size() <= qMax(qMax(fc, toc), qMax(dc, qMax(cc, mc))))
            continue;
        const QString frm = p.at(fc);
        const QString to = p.at(toc);
        if (!m_stops.contains(frm) || !m_stops.contains(to))
            continue;
        PtEdge e;
        e.toId = to;
        e.durationMin = p.at(dc).toInt();
        e.costHkd = p.at(cc).toDouble();
        e.mode = p.at(mc);
        m_adj[frm].append(e);
    }
    gf.close();
    return true;
}

int PtGraphAdvisor::directedSegmentCount() const
{
    int n = 0;
    for (auto it = m_adj.constBegin(); it != m_adj.constEnd(); ++it)
        n += it.value().size();
    return n;
}

QVector<PtJourney> PtGraphAdvisor::findJourneys(const QString &originId, const QString &destId, int maxSegments) const
{
    QVector<PtJourney> out;
    QSet<QString> seenKeys;

    QVector<PtEdge> pathEdges;
    QStringList pathNodes;
    QSet<QString> visited;

    std::function<void(const QString &)> dfs = [&](const QString &current) {
        if (current == destId) {
            const QString key = pathNodes.join(QLatin1Char('|'));
            if (!seenKeys.contains(key)) {
                seenKeys.insert(key);
                PtJourney j;
                j.edges = pathEdges;
                out.append(j);
            }
            return;
        }
        if (pathEdges.size() >= maxSegments)
            return;
        for (const PtEdge &e : m_adj.value(current)) {
            const QString &nxt = e.toId;
            if (visited.contains(nxt))
                continue;
            visited.insert(nxt);
            pathNodes.append(nxt);
            pathEdges.append(e);
            dfs(nxt);
            pathEdges.pop_back();
            pathNodes.removeLast();
            visited.remove(nxt);
        }
    };

    if (!m_stops.contains(originId) || !m_stops.contains(destId))
        return out;
    pathNodes.append(originId);
    visited.insert(originId);
    dfs(originId);
    return out;
}

void PtGraphAdvisor::rankJourneys(QVector<PtJourney> &journeys, PtPreference pref)
{
    const auto keyCheapest = [](const PtJourney &j) {
        return std::tuple<double, int, int>(j.totalCost(), j.totalDuration(), j.segmentCount());
    };
    const auto keyFastest = [](const PtJourney &j) {
        return std::tuple<int, double, int>(j.totalDuration(), j.totalCost(), j.segmentCount());
    };
    const auto keyFewSeg = [](const PtJourney &j) {
        return std::tuple<int, int, double>(j.segmentCount(), j.totalDuration(), j.totalCost());
    };
    const auto keyFewXfer = [](const PtJourney &j) {
        return std::tuple<int, int, double>(j.transferCount(), j.totalDuration(), j.totalCost());
    };

    switch (pref) {
    case PtPreference::Cheapest:
        std::sort(journeys.begin(), journeys.end(),
                  [&](const PtJourney &a, const PtJourney &b) { return keyCheapest(a) < keyCheapest(b); });
        break;
    case PtPreference::Fastest:
        std::sort(journeys.begin(), journeys.end(),
                  [&](const PtJourney &a, const PtJourney &b) { return keyFastest(a) < keyFastest(b); });
        break;
    case PtPreference::FewestSegments:
        std::sort(journeys.begin(), journeys.end(),
                  [&](const PtJourney &a, const PtJourney &b) { return keyFewSeg(a) < keyFewSeg(b); });
        break;
    case PtPreference::FewestTransfers:
        std::sort(journeys.begin(), journeys.end(),
                  [&](const PtJourney &a, const PtJourney &b) { return keyFewXfer(a) < keyFewXfer(b); });
        break;
    }
}

QString PtGraphAdvisor::formatJourneyReport(const PtJourney &j, const QString &originId,
                                            const QHash<QString, PtStop> &stops)
{
    QString s;
    const QStringList seq = j.stopSequence(originId);
    s += QStringLiteral("Route: %1\n").arg(seq.join(QStringLiteral(" -> ")));
    s += QStringLiteral("Time: %1 min, Cost: HKD %2, Transfers: %3, Segments: %4\n")
             .arg(j.totalDuration())
             .arg(j.totalCost(), 0, 'f', 2)
             .arg(j.transferCount())
             .arg(j.segmentCount());
    for (const PtEdge &e : j.edges) {
        const PtStop toSt = stops.value(e.toId);
        const QString toName = toSt.name.isEmpty() ? e.toId : toSt.name;
        s += QStringLiteral("  -> %1 (%2, %3 min, HKD %4)\n")
                 .arg(toName, e.mode)
                 .arg(e.durationMin)
                 .arg(e.costHkd, 0, 'f', 2);
    }
    return s;
}

QString PtGraphAdvisor::summaryText() const
{
    QString s;
    s += QStringLiteral("Number of stops: %1\n").arg(m_stops.size());
    s += QStringLiteral("Number of directed segments: %1\n").arg(directedSegmentCount());
    QHash<QString, int> modeCnt;
    for (auto it = m_adj.constBegin(); it != m_adj.constEnd(); ++it) {
        for (const PtEdge &e : it.value())
            modeCnt[e.mode] += 1;
    }
    for (auto it = modeCnt.constBegin(); it != modeCnt.constEnd(); ++it)
        s += QStringLiteral("  %1: %2 segments\n").arg(it.key()).arg(it.value());
    return s;
}
