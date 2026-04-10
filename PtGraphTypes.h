#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct PtStop {
    QString id;
    QString name;
    QString region;   // optional column; may mirror "type" if no region in file
    QString stopType; // MTR / Bus / etc. from CSV "type"
};

struct PtEdge {
    QString toId;
    int durationMin = 0;
    double costHkd = 0.0;
    QString mode;
};

struct PtJourney {
    QVector<PtEdge> edges;

    int totalDuration() const;
    double totalCost() const;
    int segmentCount() const { return edges.size(); }
    /** Mode change between consecutive segments counts as one transfer. */
    int transferCount() const;
    QStringList stopSequence(const QString &originId) const;
};

enum class PtPreference {
    Cheapest,
    Fastest,
    FewestSegments,
    FewestTransfers,
};

PtPreference parsePreference(const QString &s, bool *ok = nullptr);
