#include "OpenSkyParse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace {

QString strField(const QJsonObject &o, const char *key)
{
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isString() ? v.toString().trimmed() : QString();
}

QDateTime timeField(const QJsonObject &o, const char *key)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (!v.isDouble()) return QDateTime();
    return QDateTime::fromSecsSinceEpoch(qint64(v.toDouble()), Qt::UTC);
}

void noteIcao(QStringList *list, const QString &icao)
{
    if (!list || icao.isEmpty()) return;
    const QString u = icao.toUpper();
    if (!list->contains(u)) list->append(u);
}

} // namespace

bool parseFlightArray(const QByteArray &body, FlightBoard *out, QString *err,
                      QStringList *icaosSeen)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    if (pe.error != QJsonParseError::NoError) {
        if (err) *err = QStringLiteral("malformed JSON: ") + pe.errorString();
        return false;
    }
    if (!doc.isArray()) {
        if (err) *err = QStringLiteral("expected a JSON array");
        return false;
    }
    FlightBoard b;
    QStringList seen;
    for (const QJsonValue &v : doc.array()) {
        if (!v.isObject()) continue;       // skip junk entries, keep the rest
        const QJsonObject o = v.toObject();
        FlightEntry e;
        e.icao24   = strField(o, "icao24").toLower();
        e.callsign = strField(o, "callsign");
        e.estDepartureIcao = strField(o, "estDepartureAirport").toUpper();
        e.estArrivalIcao   = strField(o, "estArrivalAirport").toUpper();
        e.firstSeen = timeField(o, "firstSeen");
        e.lastSeen  = timeField(o, "lastSeen");
        noteIcao(&seen, e.estDepartureIcao);
        noteIcao(&seen, e.estArrivalIcao);
        b.flights.append(e);
    }
    b.known = true;
    if (out) *out = b;
    if (icaosSeen) for (const QString &s : seen) noteIcao(icaosSeen, s);
    return true;
}

bool parseBoard(int httpStatus, const QByteArray &body, FlightBoard *out,
                QString *err, QStringList *icaosSeen)
{
    if (httpStatus == 200)
        return parseFlightArray(body, out, err, icaosSeen);

    if (httpStatus == 404) {
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (doc.isArray() && doc.array().isEmpty()) {
            if (out) { out->known = true; out->flights.clear(); }
            return true;
        }
        if (err) *err = QStringLiteral("HTTP 404 with unexpected body");
        return false;
    }
    if (err) *err = QStringLiteral("HTTP %1").arg(httpStatus);
    return false;
}
