#include "LocalBusCsvModel.h"

#include "HkGridToWgs84.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMultiHash>
#include <QRandomGenerator>
#include <QSet>

#include <algorithm>

namespace {

double dist2(double lat1, double lng1, double lat2, double lng2)
{
    const double dlat = lat1 - lat2;
    const double dlng = lng1 - lng2;
    return dlat * dlat + dlng * dlng;
}

} // namespace

QStringList LocalBusCsvModel::splitCsvLine(const QString &line)
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
    return fields;
}

QHash<QString, int> LocalBusCsvModel::headerToIndex(const QStringList &header)
{
    QHash<QString, int> m;
    for (int i = 0; i < header.size(); ++i)
        m.insert(header.at(i).trimmed(), i);
    return m;
}

QString LocalBusCsvModel::pickCsvDirectory() const
{
    if (!m_csvOverride.isEmpty()) {
        const QString p = m_csvOverride;
        if (QFileInfo::exists(p + QStringLiteral("/STOP_BUS.csv")))
            return p;
    }
    if (!qEnvironmentVariableIsEmpty("TRANSPORT_ADVISOR_CSV_DIR")) {
        const QString p = qEnvironmentVariable("TRANSPORT_ADVISOR_CSV_DIR");
        if (QFileInfo::exists(p + QStringLiteral("/STOP_BUS.csv")))
            return p;
    }
    QDir cursor(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 12; ++i) {
        const QString candidate = cursor.absoluteFilePath(QStringLiteral("Data/Bus,MiniBus,Tram/CSV"));
        if (QFileInfo::exists(candidate + QStringLiteral("/STOP_BUS.csv")))
            return candidate;
        if (!cursor.cdUp())
            break;
    }
    return {};
}

bool LocalBusCsvModel::coordsForStop(int stopId, double *lat, double *lng) const
{
    if (!m_stopLatLng.contains(stopId))
        return false;
    const auto p = m_stopLatLng.value(stopId);
    *lat = p.first;
    *lng = p.second;
    return true;
}

bool LocalBusCsvModel::loadStops(const QString &path, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *err = QStringLiteral("Cannot open STOP_BUS.csv");
        return false;
    }
    const QString headerLine = QString::fromUtf8(f.readLine()).trimmed();
    const QHash<QString, int> col = headerToIndex(splitCsvLine(headerLine));
    const int iId = col.value(QStringLiteral("STOP_ID"), -1);
    const int iX = col.value(QStringLiteral("X"), -1);
    const int iY = col.value(QStringLiteral("Y"), -1);
    if (iId < 0 || iX < 0 || iY < 0) {
        *err = QStringLiteral("STOP_BUS.csv: missing STOP_ID/X/Y columns");
        return false;
    }
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty())
            continue;
        const QStringList p = splitCsvLine(line);
        if (p.size() <= qMax(iX, iY))
            continue;
        const int sid = p.at(iId).toInt();
        const double e = p.at(iX).toDouble();
        const double n = p.at(iY).toDouble();
        double la, ln;
        hkGridToWgs84Approx(e, n, &la, &ln);
        m_stopLatLng.insert(sid, qMakePair(la, ln));
    }
    return true;
}

bool LocalBusCsvModel::loadRouteMeta(const QString &path, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *err = QStringLiteral("Cannot open ROUTE_BUS.csv");
        return false;
    }
    const QString headerLine = QString::fromUtf8(f.readLine()).trimmed();
    const QHash<QString, int> col = headerToIndex(splitCsvLine(headerLine));
    const int iId = col.value(QStringLiteral("ROUTE_ID"), -1);
    const int iName = col.value(QStringLiteral("ROUTE_NAMEE"), -1);
    const int iTime = col.value(QStringLiteral("JOURNEY_TIME"), -1);
    const int iFare = col.value(QStringLiteral("FULL_FARE"), -1);
    const int iEnd = col.value(QStringLiteral("LOC_END_NAMEE"), -1);
    if (iId < 0 || iName < 0) {
        *err = QStringLiteral("ROUTE_BUS.csv: missing ROUTE_ID/ROUTE_NAMEE");
        return false;
    }
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty())
            continue;
        const QStringList p = splitCsvLine(line);
        if (p.size() <= iId || p.size() <= iName)
            continue;
        RouteMeta m;
        m.routeDbId = p.at(iId).toInt();
        m.routeName = p.at(iName).trimmed();
        m.journeyMinutes = iTime >= 0 && iTime < p.size() ? p.at(iTime).toInt() : 45;
        m.fullFare = iFare >= 0 && iFare < p.size() ? p.at(iFare).toDouble() : 0.0;
        m.terminusE = iEnd >= 0 && iEnd < p.size() ? p.at(iEnd).trimmed() : QString();
        m_routeById.insert(m.routeDbId, m);
    }
    return true;
}

bool LocalBusCsvModel::loadRstop(const QString &path, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *err = QStringLiteral("Cannot open RSTOP_BUS.csv");
        return false;
    }
    const QString headerLine = QString::fromUtf8(f.readLine()).trimmed();
    const QHash<QString, int> col = headerToIndex(splitCsvLine(headerLine));
    const int iRid = col.value(QStringLiteral("ROUTE_ID"), -1);
    const int iRseq = col.value(QStringLiteral("ROUTE_SEQ"), -1);
    const int iSseq = col.value(QStringLiteral("STOP_SEQ"), -1);
    const int iSid = col.value(QStringLiteral("STOP_ID"), -1);
    const int iNe = col.value(QStringLiteral("STOP_NAMEE"), -1);
    if (iRid < 0 || iRseq < 0 || iSseq < 0 || iSid < 0 || iNe < 0) {
        *err = QStringLiteral("RSTOP_BUS.csv: missing expected columns");
        return false;
    }

    struct Row {
        int sseq;
        int sid;
        QString name;
    };
    QHash<QString, QVector<Row>> buckets;

    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty())
            continue;
        const QStringList p = splitCsvLine(line);
        if (p.size() <= qMax(qMax(iRid, iRseq), qMax(iSseq, qMax(iSid, iNe))))
            continue;
        const int rid = p.at(iRid).toInt();
        const int rseq = p.at(iRseq).toInt();
        Row r;
        r.sseq = p.at(iSseq).toInt();
        r.sid = p.at(iSid).toInt();
        r.name = p.at(iNe).trimmed();
        const QString k = QStringLiteral("%1:%2").arg(rid).arg(rseq);
        buckets[k].append(r);
    }

    for (auto it = buckets.begin(); it != buckets.end(); ++it) {
        auto &rows = it.value();
        std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.sseq < b.sseq; });
        QVector<RouteStopPt> ordered;
        ordered.reserve(rows.size());
        for (const Row &r : rows) {
            ordered.append({r.sid, r.name});
            m_stopIdToRouteKeys.insert(r.sid, it.key());
        }
        m_stopsOnRoute.insert(it.key(), ordered);
    }
    return true;
}

bool LocalBusCsvModel::ensureLoaded(QString *errorMessage)
{
    if (m_loaded)
        return true;
    m_csvDir = pickCsvDirectory();
    if (m_csvDir.isEmpty()) {
        *errorMessage =
            QStringLiteral("Could not find Data/Bus,MiniBus,Tram/CSV (STOP_BUS.csv). "
                           "Set env TRANSPORT_ADVISOR_CSV_DIR to that folder, or run the app from "
                           "a folder that contains Mapper/Data/...");
        return false;
    }
    QString err;
    if (!loadStops(m_csvDir + QStringLiteral("/STOP_BUS.csv"), &err)) {
        *errorMessage = err;
        return false;
    }
    if (!loadRouteMeta(m_csvDir + QStringLiteral("/ROUTE_BUS.csv"), &err)) {
        *errorMessage = err;
        return false;
    }
    if (!loadRstop(m_csvDir + QStringLiteral("/RSTOP_BUS.csv"), &err)) {
        *errorMessage = err;
        return false;
    }
    m_loaded = true;
    return true;
}

QJsonArray LocalBusCsvModel::pathJsonForSlice(const QVector<RouteStopPt> &slice) const
{
    QJsonArray arr;
    for (const RouteStopPt &s : slice) {
        double la, ln;
        if (!coordsForStop(s.stopId, &la, &ln))
            continue;
        QJsonArray pair;
        pair.append(la);
        pair.append(ln);
        arr.append(pair);
    }
    return arr;
}

QJsonObject LocalBusCsvModel::routeToJson(const RouteMeta &meta, const QVector<RouteStopPt> &slice,
                                          int segCount, int estDurationSec) const
{
    const QJsonArray path = pathJsonForSlice(slice);
    QString endName = meta.terminusE;
    if (!slice.isEmpty())
        endName = slice.last().nameEn;

    const QString fareTxt = meta.fullFare > 0
                                ? QStringLiteral("HK$ %1").arg(meta.fullFare, 0, 'f', 1)
                                : QStringLiteral("Not provided");

    QJsonObject o;
    o.insert(QStringLiteral("summary"),
             QStringLiteral("Bus route %1 (CSV demo)").arg(meta.routeName));
    o.insert(QStringLiteral("durationSec"), estDurationSec);
    o.insert(QStringLiteral("durationText"),
             QStringLiteral("~%1 min (estimated from timetable column)").arg(qMax(1, estDurationSec / 60)));
    o.insert(QStringLiteral("fareText"), fareTxt);
    o.insert(QStringLiteral("fareSort"), meta.fullFare > 0 ? meta.fullFare : 1.0e9);
    o.insert(QStringLiteral("transfers"), segCount);
    o.insert(QStringLiteral("polyline"), QString());
    o.insert(QStringLiteral("path"), path);
    o.insert(QStringLiteral("startAddress"),
             slice.isEmpty() ? QString() : slice.first().nameEn);
    o.insert(QStringLiteral("endAddress"), endName);
    o.insert(QStringLiteral("source"), QStringLiteral("local_csv"));
    return o;
}

QJsonObject LocalBusCsvModel::buildRankedPayload(const QVector<QJsonObject> &routes) const
{
    QJsonArray base;
    for (const QJsonObject &o : routes)
        base.append(o);

    auto sortKey = [](QJsonArray arr, const QString &key, bool ascending) {
        QList<QJsonObject> rows;
        rows.reserve(arr.size());
        for (const QJsonValue &v : arr)
            rows.append(v.toObject());
        std::sort(rows.begin(), rows.end(), [&](const QJsonObject &a, const QJsonObject &b) {
            const double va = a.value(key).toDouble();
            const double vb = b.value(key).toDouble();
            return ascending ? (va < vb) : (va > vb);
        });
        QJsonArray out;
        for (const QJsonObject &o : rows)
            out.append(o);
        return out;
    };

    QJsonObject out;
    out.insert(QStringLiteral("byTime"), sortKey(base, QStringLiteral("durationSec"), true));
    out.insert(QStringLiteral("byCost"), sortKey(base, QStringLiteral("fareSort"), true));
    out.insert(QStringLiteral("byTransfers"), sortKey(base, QStringLiteral("transfers"), true));
    out.insert(QStringLiteral("rawRouteCount"), routes.size());
    out.insert(QStringLiteral("source"), QStringLiteral("hk_bus_csv"));
    return out;
}

QJsonObject LocalBusCsvModel::randomPreviewPayload()
{
    QStringList keys = m_stopsOnRoute.keys();
    std::shuffle(keys.begin(), keys.end(), *QRandomGenerator::global());

    QVector<QJsonObject> picked;
    for (const QString &k : keys) {
        if (picked.size() >= 5)
            break;
        const QVector<RouteStopPt> &stops = m_stopsOnRoute.value(k);
        if (stops.size() < 3)
            continue;
        const QStringList ps = k.split(QLatin1Char(':'));
        const int routeId = ps.at(0).toInt();
        const RouteMeta meta = m_routeById.value(routeId);
        if (meta.routeDbId == 0)
            continue;

        const int n = stops.size();
        const int a = QRandomGenerator::global()->bounded(n - 1);
        const int b = a + 1 + QRandomGenerator::global()->bounded(n - 1 - a);
        const int lo = qMin(a, b);
        const int hi = qMax(a, b);
        QVector<RouteStopPt> slice;
        for (int i = lo; i <= hi; ++i)
            slice.append(stops.at(i));

        const double frac = double(slice.size()) / double(qMax(1, n));
        int estSec = int(meta.journeyMinutes * 60.0 * frac);
        estSec += QRandomGenerator::global()->bounded(180) - 90;
        estSec = qMax(120, estSec);

        const int segs = qMax(1, slice.size() - 1);
        picked.append(routeToJson(meta, slice, segs, estSec));
    }

    if (picked.isEmpty())
        return QJsonObject();

    return buildRankedPayload(picked);
}

QJsonObject LocalBusCsvModel::suggestFromDestination(const QString &destQuery, double originLat,
                                                     double originLng)
{
    const QString q = destQuery.trimmed().toLower();
    QVector<int> destStopCandidates;
    destStopCandidates.reserve(200);
    for (auto it = m_stopsOnRoute.constBegin(); it != m_stopsOnRoute.constEnd(); ++it) {
        for (const RouteStopPt &s : it.value()) {
            if (q.isEmpty())
                continue;
            if (s.nameEn.toLower().contains(q))
                destStopCandidates.append(s.stopId);
        }
    }

    if (destStopCandidates.isEmpty() && !q.isEmpty()) {
        for (auto it = m_routeById.constBegin(); it != m_routeById.constEnd(); ++it) {
            if (it.value().terminusE.toLower().contains(q)) {
                const int rid = it.key();
                for (auto rt = m_stopsOnRoute.constBegin(); rt != m_stopsOnRoute.constEnd(); ++rt) {
                    const QStringList ps = rt.key().split(QLatin1Char(':'));
                    if (ps.size() == 2 && ps[0].toInt() == rid && !rt.value().isEmpty())
                        destStopCandidates.append(rt.value().last().stopId);
                }
            }
        }
    }

    if (destStopCandidates.isEmpty()) {
        for (auto it = m_stopLatLng.constBegin(); it != m_stopLatLng.constEnd(); ++it)
            destStopCandidates.append(it.key());
    }

    std::shuffle(destStopCandidates.begin(), destStopCandidates.end(), *QRandomGenerator::global());
    const int destStopId = destStopCandidates.isEmpty() ? -1 : destStopCandidates.first();

    int originStopId = -1;
    double bestD = 1e300;
    for (auto it = m_stopLatLng.constBegin(); it != m_stopLatLng.constEnd(); ++it) {
        const double d = dist2(originLat, originLng, it.value().first, it.value().second);
        if (d < bestD) {
            bestD = d;
            originStopId = it.key();
        }
    }

    QSet<QString> triedKeys;
    QVector<QJsonObject> outObjs;

    auto trySlice = [&](const QString &routeKey, int i0, int i1) {
        if (i0 > i1)
            std::swap(i0, i1);
        const QVector<RouteStopPt> &all = m_stopsOnRoute.value(routeKey);
        if (all.isEmpty() || i0 < 0 || i1 >= all.size())
            return;
        QVector<RouteStopPt> slice;
        for (int i = i0; i <= i1; ++i)
            slice.append(all.at(i));
        const QStringList ps = routeKey.split(QLatin1Char(':'));
        const int routeId = ps.at(0).toInt();
        const RouteMeta meta = m_routeById.value(routeId);
        if (meta.routeDbId == 0)
            return;
        const double frac = double(slice.size()) / double(qMax(1, all.size()));
        int estSec = int(meta.journeyMinutes * 60.0 * frac);
        estSec = qMax(180, estSec);
        const int segs = qMax(1, slice.size() - 1);
        outObjs.append(routeToJson(meta, slice, segs, estSec));
    };

    if (destStopId >= 0 && originStopId >= 0) {
        const QList<QString> destRoutes = m_stopIdToRouteKeys.values(destStopId);
        QList<QString> originRoutes = m_stopIdToRouteKeys.values(originStopId);
        QSet<QString> originSet(originRoutes.begin(), originRoutes.end());
        for (const QString &rk : destRoutes) {
            if (!originSet.contains(rk) || triedKeys.contains(rk))
                continue;
            const QVector<RouteStopPt> &all = m_stopsOnRoute.value(rk);
            int io = -1, id = -1;
            for (int i = 0; i < all.size(); ++i) {
                if (all.at(i).stopId == originStopId)
                    io = i;
                if (all.at(i).stopId == destStopId)
                    id = i;
            }
            if (io >= 0 && id >= 0 && io < id) {
                triedKeys.insert(rk);
                trySlice(rk, io, id);
            }
        }
    }

    if (outObjs.size() < 3 && destStopId >= 0) {
        const QList<QString> destRoutes = m_stopIdToRouteKeys.values(destStopId);
        QList<QString> shuffled = destRoutes;
        std::shuffle(shuffled.begin(), shuffled.end(), *QRandomGenerator::global());
        for (const QString &rk : shuffled) {
            if (outObjs.size() >= 5)
                break;
            if (triedKeys.contains(rk))
                continue;
            const QVector<RouteStopPt> &all = m_stopsOnRoute.value(rk);
            int id = -1;
            for (int i = 0; i < all.size(); ++i) {
                if (all.at(i).stopId == destStopId) {
                    id = i;
                    break;
                }
            }
            if (id < 0)
                continue;
            triedKeys.insert(rk);
            trySlice(rk, 0, id);
        }
    }

    for (int attempt = 0; outObjs.size() < 3 && attempt < 40; ++attempt) {
        QStringList keys = m_stopsOnRoute.keys();
        if (keys.isEmpty())
            break;
        std::shuffle(keys.begin(), keys.end(), *QRandomGenerator::global());
        const QString rk = keys.at(QRandomGenerator::global()->bounded(keys.size()));
        if (triedKeys.contains(rk))
            continue;
        triedKeys.insert(rk);
        const QVector<RouteStopPt> &all = m_stopsOnRoute.value(rk);
        if (all.size() < 2)
            continue;
        const int hi = qMin(all.size() - 1, 5 + QRandomGenerator::global()->bounded(qMax(1, all.size() - 1)));
        trySlice(rk, 0, hi);
    }

    if (outObjs.isEmpty())
        return randomPreviewPayload();

    return buildRankedPayload(outObjs);
}
