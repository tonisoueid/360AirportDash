#include "AirportSearch.h"

QVector<Airport> searchAirports(const QVector<Airport> &airports,
                                const QString &query, int limit)
{
    QVector<Airport> out;
    const QString q = query.trimmed();
    if (q.isEmpty() || limit <= 0)
        return out;

    for (const Airport &a : airports) {
        const QString *fields[] = {&a.icao, &a.iata, &a.name, &a.city, &a.country};
        for (const QString *f : fields) {
            if (!f->isEmpty() && f->contains(q, Qt::CaseInsensitive)) {
                out.push_back(a);
                break;
            }
        }
        if (out.size() >= limit)
            break;
    }
    return out;
}

QString iataToIcao(const QVector<Airport> &airports, const QString &text)
{
    const QString t = text.trimmed();
    if (t.size() != 3)
        return QString();
    for (const QChar c : t)
        if (!c.isLetter() || c.unicode() > 127)
            return QString();
    for (const Airport &a : airports)
        if (!a.iata.isEmpty() && a.iata.compare(t, Qt::CaseInsensitive) == 0)
            return a.icao;
    return QString();
}
