#pragma once

#include "SiteData.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QVector>

// ----------------------------------------------------------------------------
// Polls the watched sites on a staggered scan cycle, like a NOC polling its
// telemetry stations in round-robin. One in-flight request per site; failures
// degrade the site to Error/Stale without touching the last good sample. The
// roster is hot-swappable: setSites() rebuilds the polling list while
// preserving the last good sample of any site whose code and endpoint are
// unchanged, and drops replies that were in flight for a superseded roster.
//
// THE FEED SEAM: pollSite() ships as a stub — no data source is wired, so
// sites stay Pending and render explicit gaps ("AWAITING FIRST SCAN").
// Replace the marked block in FeedService.cpp with your transport, then call
// applySample() / applyFailure(); everything downstream (cards, chart, alert
// rules, event log) lights up with no further changes. A worked example
// lives in docs/recipes.md "Wire a feed".
// ----------------------------------------------------------------------------
class FeedService : public QObject
{
    Q_OBJECT
public:
    explicit FeedService(const QVector<Site> &sites, QObject *parent = nullptr);

    void start();                        // kick off the first scan
    void rescan();                       // full sweep now
    void pollSite(int idx);              // single-site poll (FEED SEAM)
    void setSites(const QVector<Site> &sites);   // hot roster swap

    int siteCount() const { return int(m_data.size()); }
    const SiteData &data(int idx) const { return m_data[idx]; }
    const QVector<SiteData> &all() const { return m_data; }

    int scanDone() const { return m_scanDone; }
    int scanTotal() const { return m_scanTotal; }
    bool scanActive() const { return m_scanTimer.isActive() || m_scanQueueRemaining > 0; }
    QDateTime lastFullSyncUtc() const { return m_lastSync; }

    static constexpr int kRefreshSecs = 900;   // scheduled sweep every 15 min
    static constexpr int kStaggerMs = 450;     // cadence between site polls

signals:
    void siteUpdated(int idx);
    void scanProgress(int done, int total);
    void linkStatusChanged(bool online);
    void sitesReconfigured();             // roster swapped; indexes are new

    // Airport-directory hooks (see docs/architecture.md "Airport learning").
    // Emitted only when the feed actually reports something.
    void airportReported(const QString &icao);   // feed named this ICAO
    void airportFillCompleted();                 // a requested fill finished

public:
    // -- seam API: call these from your poll implementation -----------------

    // Apply a freshly measured sample: stamps it, marks the site Live,
    // appends to the trailing history (capped), raises the link flag, and
    // completes the poll's scan-queue bookkeeping.
    void applySample(int idx, const Sample &s);

    // Apply a failure: keeps the last good sample, flags the site
    // Stale (was Live) or Error (never answered), drops the link flag, and
    // completes the poll's scan-queue bookkeeping.
    void applyFailure(int idx, const QString &error);

    // Airport seam: call from your poll/client code whenever a sample or
    // state names an ICAO. Normalises (trim, upper-case), drops blanks, and
    // emits airportReported(); the listener decides whether it is unknown.
    void airportSeen(const QString &icao);

    // Ask the feed to refill the airport directory (used after a damaged
    // airports.json). STUB: no client is wired, so nothing is requested and
    // airportFillCompleted() is never emitted. A real client calls
    // completeAirportFill() once its fill has finished.
    void requestAirportFill();
    void completeAirportFill();

private:
    void advanceScan();
    void finishPoll(int idx);             // scan-queue bookkeeping + signal

    QVector<SiteData> m_data;
    QNetworkAccessManager m_net;          // ready for the seam (transfer
                                          // timeout armed in the ctor)
    QTimer m_scanTimer;                   // walks the staggered sweep
    int m_scanCursor = 0;
    int m_scanQueueRemaining = 0;
    int m_scanDone = 0;
    int m_scanTotal = 0;
    QDateTime m_lastSync;
    bool m_linkOnline = true;
    bool m_started = false;
    quint32 m_generation = 0;             // bumped on roster swap; retires in-flight replies
};
