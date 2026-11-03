#pragma once

#include "Airport.h"
#include <QVector>

// Compiled-in table of 300 of the world's busiest / best-known airports.
// Every entry has ICAO, IATA, name, city, country, IANA time zone and
// coordinates, and is flagged seeded = true. ICAO codes are unique.
const QVector<Airport> &airportSeed();
