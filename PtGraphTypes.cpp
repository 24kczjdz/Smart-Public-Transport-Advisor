#include "PtGraphTypes.h"

int PtJourney::totalDuration() const
{
    int s = 0;
    for (const PtEdge &e : edges)
        s += e.durationMin;
    return s;
}

double PtJourney::totalCost() const
{
    double s = 0.0;
    for (const PtEdge &e : edges)
        s += e.costHkd;
    return s;
}

int PtJourney::transferCount() const
{
    if (edges.size() <= 1)
        return 0;
    int t = 0;
    for (int i = 1; i < edges.size(); ++i) {
        if (edges.at(i).mode != edges.at(i - 1).mode)
            ++t;
    }
    return t;
}

QStringList PtJourney::stopSequence(const QString &originId) const
{
    QStringList seq;
    seq.append(originId);
    for (const PtEdge &e : edges)
        seq.append(e.toId);
    return seq;
}

PtPreference parsePreference(const QString &raw, bool *ok)
{
    if (ok)
        *ok = true;
    const QString s = raw.trimmed().toLower();
    if (s == QLatin1String("cheapest") || s == QLatin1String("cost"))
        return PtPreference::Cheapest;
    if (s == QLatin1String("fastest") || s == QLatin1String("time"))
        return PtPreference::Fastest;
    if (s == QLatin1String("fewest_segments") || s == QLatin1String("fewest_stops") || s == QLatin1String("segments"))
        return PtPreference::FewestSegments;
    if (s == QLatin1String("fewest_transfers") || s == QLatin1String("transfers"))
        return PtPreference::FewestTransfers;
    if (ok)
        *ok = false;
    return PtPreference::Fastest;
}
