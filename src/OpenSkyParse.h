#pragma once

#include "SiteData.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

// Pure OpenSky /flights/arrival and /flights/departure parsing (no network).

// Parse a JSON array body into `out` (known=true on success). Null or missing
// fields become empty/invalid. Every non-empty ICAO seen (estimated
// departure/arrival airports) is appended, de-duplicated and uppercased, to
// `icaosSeen` when non-null — feed these to FeedService::airportSeen().
// Returns false and sets *err on malformed JSON or a non-array document;
// `out` is left untouched on failure.
bool parseFlightArray(const QByteArray &body, FlightBoard *out, QString *err,
                      QStringList *icaosSeen = nullptr);

// HTTP-aware wrapper. Rules:
//   200            -> parseFlightArray ("[]" is a known, empty board)
//   404 + "[]"     -> VALID known empty board (OpenSky's "no flights" reply)
//   404 otherwise  -> error
//   other statuses -> error
bool parseBoard(int httpStatus, const QByteArray &body, FlightBoard *out,
                QString *err, QStringList *icaosSeen = nullptr);
