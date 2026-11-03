#pragma once

#include <QString>
#include <cmath>

// ----------------------------------------------------------------------------
// One airport. Follows the repo's missing-data convention: strings are empty
// when unknown, doubles are NaN when unknown — consumers must check
// (std::isnan / isEmpty) before use; never substitute 0.
// ----------------------------------------------------------------------------
struct Airport {
    QString icao;      // 4-letter ICAO location indicator, "EGLL" (unique key)
    QString iata;      // 3-letter IATA code, "LHR" (empty if none)
    QString name;      // "London Heathrow Airport"
    QString city;      // "London"
    QString country;   // "United Kingdom"
    QString timeZone;  // IANA zone id, "Europe/London"
    double  lat = std::nan("");  // degrees, WGS84, +N
    double  lon = std::nan("");  // degrees, WGS84, +E
    bool    seeded = false;      // true when it came from the compiled-in table
};
