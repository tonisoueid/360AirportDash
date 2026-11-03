#pragma once

#include "Airport.h"

#include <QString>
#include <QVector>

// Text shown by the UI when a search yields nothing.
inline QString noAirportsFoundText() { return QStringLiteral("no airports found"); }

// Pure local search: case-insensitive substring match across ICAO, IATA, name,
// city and country (empty fields are skipped). Returns at most `limit` matches
// in input order. An empty/blank query returns nothing. No I/O, no network.
QVector<Airport> searchAirports(const QVector<Airport> &airports,
                                const QString &query, int limit = 10);
