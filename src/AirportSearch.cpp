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
