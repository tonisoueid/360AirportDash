#pragma once

#include "SiteData.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QVector>

#include <functional>
#include <memory>

class OpenSkyAuth;

// ----------------------------------------------------------------------------
// Polls the watched sites on a staggered scan cycle, like a NOC polling its
// telemetry stations in round-robin. One in-flight request per site; failures
// degrade the site to Error/Stale without touching the last good sample. The
// roster is hot-swappable: setSites() rebuilds the polling list while
// preserving the last good sample of any site whose code and endpoint are
// unchanged, and drops replies that were in flight for a superseded roster.
//
// THE FEED: pollSite() is wired to the OpenSky flights API (arrivals and
// departures for the site's ICAO over a trailing window). Authenticated mode
// (OpenSkyAuth bearer token) fetches both boards; anonymous mode sends no
// auth header and fetches DEPARTURES ONLY, leaving arrivals unknown (gap).
// Every poll ends in exactly one applySample()/applyFailure(). HTTP 429
// honours Retry-After and pauses all OpenSky requests; HTTP 5xx is retried
// once. See docs/architecture.md "OpenSky feed" and docs/recipes.md.
// ----------------------------------------------------------------------------
class FeedService : public QObject
{
    Q_OBJECT
public:
    explicit FeedService(const QVector<Site> &sites, QObject *parent = nullptr);

    void start();                        // kick off the first scan
    void rescan();                       // full sweep now
    void pollSite(int idx);              // single-site poll (FEED SEAM)
    // Real re-poll of one site (detail-pane refresh). Returns false (no-op)
    // for a bad index, a poll already in flight, or an active 429 pause.
    // Does not count toward scan progress. Callers must not use it offline.
    bool refreshSite(int idx);
    void setSites(const QVector<Site> &sites);   // hot roster swap

    int siteCount() const { return int(m_data.size()); }
    const SiteData &data(int idx) const { return m_data[idx]; }
    const QVector<SiteData> &all() const { return m_data; }

    int scanDone() const { return m_scanDone; }
    int scanTotal() const { return m_scanTotal; }
    bool scanActive() const { return m_scanTimer.isActive() || m_scanQueueRemaining > 0; }
    QDateTime lastFullSyncUtc() const { return m_lastSync; }

    // OpenSky access. The base URL / retry delay are test seams.
    OpenSkyAuth *openSkyAuth() const { return m_auth; }
    void setBaseUrl(const QString &url) { m_baseUrl = url; }
    void setRetryDelayMs(int ms) { m_retryDelayMs = ms; }

    // True only while a real 429 Retry-After pause is in force.
    bool rateLimitPaused() const;

    static constexpr int kWindowSecs = 24 * 3600;   // trailing board window
    static constexpr int kMax429Retries = 3;        // per board, then fail
    static constexpr int kRefreshSecs = 900;   // scheduled sweep every 15 min
    static constexpr int kStaggerMs = 450;     // cadence between site polls

signals:
    void siteUpdated(int idx);
    void scanProgress(int done, int total);
    void linkStatusChanged(bool online);
    void sitesReconfigured();             // roster swapped; indexes are new
    void rateLimitPausedChanged(bool paused);   // 429 pause began / lapsed

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
    // airports.json). STUB: the OpenSky client has no airport-list fill, so
    // nothing is requested and airportFillCompleted() is never emitted. A real client calls
    // completeAirportFill() once its fill has finished.
    void requestAirportFill();
    void completeAirportFill();

private:
    struct BoardResult {
        bool done = false;            // finished (ok, failed, or skipped)
        bool ok = false;              // measured -> `board.known`
        FlightBoard board;
        QString err;
        int tries5xx = 0;
        int tries429 = 0;
    };
    struct PollJob {
        int idx = -1;
        quint32 gen = 0;
        QString icao;
        QString token;                // bearer; empty in anonymous mode
        bool anon = true;
        BoardResult dep, arr;
        QStringList icaos;
    };
    struct PollState {
        bool busy = false;            // one in-flight poll per site
        bool manual = false;          // refreshSite() poll: skip scan bookkeeping
        QTimer *timer = nullptr;      // per-site retry / rate-limit wait
        std::function<void()> onTimer;
    };

    void resetPollState();
    void advanceJob(const std::shared_ptr<PollJob> &job);
    void issueBoard(const std::shared_ptr<PollJob> &job, bool departures);
    void armTimer(const std::shared_ptr<PollJob> &job, int ms, std::function<void()> fn);
    void completeJob(const std::shared_ptr<PollJob> &job);
    void advanceScan();
    void finishPoll(int idx, bool countsScan);   // scan-queue bookkeeping + signal
    bool takeCountsScan(int idx);
    void syncPauseState();                // emit on pause edge, arm lapse timer

    QVector<SiteData> m_data;
    QNetworkAccessManager m_net;          // transfer timeout armed in the ctor
    OpenSkyAuth *m_auth = nullptr;        // child; anonymous when no credentials
    QString m_baseUrl;                    // OpenSky REST base (no trailing slash)
    int m_retryDelayMs = 2000;            // pause before the single 5xx retry
    qint64 m_blockedUntilMs = 0;          // global 429 back-off (epoch ms)
    QTimer m_pauseTimer;                  // fires when the 429 pause lapses
    bool m_pauseReported = false;         // last value emitted
    QVector<PollState> m_poll;           // parallel to m_data
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
