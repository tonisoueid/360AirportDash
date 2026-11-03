#pragma once

#include "Airport.h"

#include <QObject>
#include <QString>
#include <QVector>

// ----------------------------------------------------------------------------
// The airport directory. Starts from the compiled-in seed table, then is
// replaced by airports.json (same config directory as sites.json) when a valid
// file exists. Unknown ICAOs learned from the feed are appended with only the
// ICAO set — no invented name or city — and persisted.
// ----------------------------------------------------------------------------
class AirportRegistry : public QObject
{
    Q_OBJECT
public:
    explicit AirportRegistry(QObject *parent = nullptr);

    enum class LoadResult { Seed, File, Corrupt };
    LoadResult load();

    // True after a corrupt/unparsable file forced a seed fallback, so the
    // feed fill can run again. Cleared by a successful save.
    bool needsRefill() const { return m_needsRefill; }

    // No-op if the ICAO is already known (case-insensitive). Otherwise appends
    // an ICAO-only entry (stored upper-case), saves, emits airportsChanged().
    // Returns true when an entry was added.
    bool addLearned(const QString &icao);

    const QVector<Airport> &airports() const { return m_airports; }

    static QString configPath();

signals:
    void airportsChanged();

private:
    bool save();
    bool contains(const QString &icaoUpper) const;

    QVector<Airport> m_airports;
    bool m_needsRefill = false;
};
