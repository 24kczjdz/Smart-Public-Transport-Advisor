#pragma once

#include "PtGraphTypes.h"

#include <QHash>
#include <QString>

/**
 * Loads stop/segment CSVs and finds simple paths with DFS (unique by stop sequence),
 * then ranks by preference — same model as the coursework Python reference.
 */
class PtGraphAdvisor {
public:
    bool loadFromFiles(const QString &stopsCsvPath, const QString &segmentsCsvPath, QString *errorOut = nullptr);

    const QHash<QString, PtStop> &stops() const { return m_stops; }
    int directedSegmentCount() const;

    QVector<PtJourney> findJourneys(const QString &originId, const QString &destId, int maxSegments = 8) const;

    static void rankJourneys(QVector<PtJourney> &journeys, PtPreference pref);
    static QString formatJourneyReport(const PtJourney &j, const QString &originId, const QHash<QString, PtStop> &stops);

    QString summaryText() const;

private:
    static QStringList splitCsvLine(const QString &line);
    static bool parseStopsHeader(const QStringList &header, int *idCol, int *nameCol, int *typeCol, int *regionCol);
    static bool parseSegmentsHeader(const QStringList &header, int *fromCol, int *toCol, int *durCol, int *costCol,
                                    int *modeCol);

    QHash<QString, PtStop> m_stops;
    QHash<QString, QVector<PtEdge>> m_adj; // from_stop -> outgoing edges
};
