#include "FeedService.h"

#include "OpenSkyAuth.h"
#include "OpenSkyParse.h"

#include <QDateTime>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include <utility>

FeedService::FeedService(const QVector<Site> &sites, QObject *parent)
    : QObject(parent)
{
    m_data.reserve(sites.size());
    for (const Site &s : sites)
        m_data.push_back(SiteData{s, {}, {}, SiteData::State::Pending, {}, {}});

    m_net.setTransferTimeout(9000);
    m_baseUrl = QStringLiteral("https://opensky-network.org/api");
    m_auth = new OpenSkyAuth(&m_net, this);
    m_auth->loadCredentials();
    resetPollState();
    connect(&m_scanTimer, &QTimer::timeout, this, &FeedService::advanceScan);
    m_pauseTimer.setSingleShot(true);
    connect(&m_pauseTimer, &QTimer::timeout, this, &FeedService::syncPauseState);
}

void FeedService::setSites(const QVector<Site> &sites)
{
    ++m_generation;                    // retire replies still in flight for the old roster
    m_scanTimer.stop();
    m_scanQueueRemaining = 0;
    m_scanCursor = 0;
    // Cancels per-site timers; replies still in flight are dropped by gen.

    QVector<SiteData> next;
    next.reserve(sites.size());
    for (const Site &s : sites) {
        SiteData d;
        d.site = s;
        // Carry the last good sample across an edit when site identity (code)
        // and feed locator are unchanged.
        for (const SiteData &old : std::as_const(m_data)) {
            if (old.site.code == s.code && old.site.endpoint == s.endpoint) {
                d = old;
                break;
            }
        }
        next.push_back(d);
    }
    m_data = next;
    resetPollState();
    emit sitesReconfigured();
    if (m_started)
        rescan();                      // pick up new / relocated sites right away
}

void FeedService::start()
{
    m_started = true;
    rescan();
}

void FeedService::rescan()
{
    m_scanCursor = 0;
    m_scanQueueRemaining = siteCount();
    m_scanDone = 0;
    m_scanTotal = siteCount();
    m_scanTimer.start(kStaggerMs);
    emit scanProgress(m_scanDone, m_scanTotal);
}

void FeedService::advanceScan()
{
    if (m_scanCursor >= siteCount()) {
        m_scanTimer.stop();
        return;
    }
    pollSite(m_scanCursor++);
}

void FeedService::resetPollState()
{
    for (PollState &ps : m_poll) {
        if (ps.timer) {
            ps.timer->stop();
            ps.timer->disconnect();
            ps.timer->deleteLater();
        }
    }
    m_poll.clear();
    m_poll.resize(m_data.size());
    for (int i = 0; i < m_poll.size(); ++i) {
        QTimer *t = new QTimer(this);
        t->setSingleShot(true);
        connect(t, &QTimer::timeout, this, [this, i, gen = m_generation] {
            if (gen != m_generation || i >= m_poll.size())
                return;
            auto fn = std::move(m_poll[i].onTimer);
            m_poll[i].onTimer = nullptr;
            if (fn)
                fn();
        });
        m_poll[i].timer = t;
    }
}

// OpenSky feed. Per poll: fetch the departures board, then (authenticated
// only) the arrivals board, sequentially so a site never has more than one
// request or wait pending. Exactly one applySample()/applyFailure() ends it.
void FeedService::pollSite(int idx)
{
    if (idx < 0 || idx >= siteCount())
        return;

    // One in-flight poll per site: a rescan landing on a busy site only
    // advances the queue bookkeeping; the running poll reports on its own.
    if (m_poll[idx].busy) {
        finishPoll(idx);
        return;
    }

    const QString icao = m_data[idx].site.code.trimmed().toUpper();
    static const QRegularExpression icaoRe(QStringLiteral("^[A-Z]{4}$"));
    if (!icaoRe.match(icao).hasMatch()) {
        applyFailure(idx, QStringLiteral("invalid ICAO code"));
        return;
    }

    auto job = std::make_shared<PollJob>();
    job->idx = idx;
    job->gen = m_generation;
    job->icao = icao;
    job->anon = !m_auth->hasCredentials();
    m_poll[idx].busy = true;

    m_auth->withToken([this, job](const QString &token, const QString &error) {
        if (job->gen != m_generation)
            return;                              // roster swapped meanwhile
        if (!job->anon && (token.isEmpty() || !error.isEmpty())) {
            m_poll[job->idx].busy = false;
            applyFailure(job->idx, error.isEmpty() ? QStringLiteral("no access token") : error);
            return;
        }
        job->token = token;
        if (job->anon) {
            // Anonymous: departures only. Arrivals stay unknown (a gap).
            job->arr.done = true;
        }
        advanceJob(job);
    });
}

void FeedService::advanceJob(const std::shared_ptr<PollJob> &job)
{
    if (job->gen != m_generation)
        return;
    if (!job->dep.done)
        issueBoard(job, true);
    else if (!job->arr.done)
        issueBoard(job, false);
    else
        completeJob(job);
}

void FeedService::armTimer(const std::shared_ptr<PollJob> &job, int ms, std::function<void()> fn)
{
    PollState &ps = m_poll[job->idx];
    ps.onTimer = std::move(fn);
    ps.timer->start(qMax(1, ms));
}

void FeedService::issueBoard(const std::shared_ptr<PollJob> &job, bool departures)
{
    // A 429 anywhere pauses every OpenSky request until the limit lapses.
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_blockedUntilMs > nowMs) {
        armTimer(job, int(m_blockedUntilMs - nowMs) + 50,
                 [this, job, departures] { issueBoard(job, departures); });
        return;
    }

    const qint64 end = nowMs / 1000;
    QUrl url(m_baseUrl + (departures ? QStringLiteral("/flights/departure")
                                     : QStringLiteral("/flights/arrival")));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("airport"), job->icao);
    q.addQueryItem(QStringLiteral("begin"), QString::number(end - kWindowSecs));
    q.addQueryItem(QStringLiteral("end"), QString::number(end));
    url.setQuery(q);

    QNetworkRequest req(url);
    if (!job->token.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + job->token.toUtf8());

    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, job, departures, reply] {
        reply->deleteLater();
        if (job->gen != m_generation)
            return;                              // roster swapped: drop

        BoardResult &b = departures ? job->dep : job->arr;
        const QString what = departures ? QStringLiteral("departures") : QStringLiteral("arrivals");
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        auto failBoard = [&](const QString &msg) {
            b.done = true;
            b.ok = false;
            b.err = QStringLiteral("OpenSky %1: %2").arg(what, msg);
            advanceJob(job);
        };

        if (status == 429) {
            QByteArray ra = reply->rawHeader("Retry-After");
            if (ra.isEmpty())
                ra = reply->rawHeader("X-Rate-Limit-Retry-After-Seconds");
            bool ok = false;
            int secs = ra.trimmed().toInt(&ok);
            if (!ok || secs < 1)
                secs = 60;
            secs = qMin(secs, 3600);
            m_blockedUntilMs = qMax(m_blockedUntilMs,
                                    QDateTime::currentMSecsSinceEpoch() + qint64(secs) * 1000);
            syncPauseState();
            if (++b.tries429 > kMax429Retries) {
                failBoard(QStringLiteral("rate limited (HTTP 429)"));
                return;
            }
            armTimer(job, secs * 1000, [this, job, departures] { issueBoard(job, departures); });
            return;
        }
        if (status >= 500 && status < 600) {
            if (b.tries5xx++ == 0) {
                armTimer(job, m_retryDelayMs, [this, job, departures] { issueBoard(job, departures); });
                return;
            }
            failBoard(QStringLiteral("HTTP %1").arg(status));
            return;
        }
        if (status == 0) {
            failBoard(reply->error() != QNetworkReply::NoError ? reply->errorString()
                                                               : QStringLiteral("no HTTP response"));
            return;
        }

        FlightBoard fb;
        QString err;
        QStringList seen;
        if (!parseBoard(status, reply->readAll(), &fb, &err, &seen)) {
            failBoard(err.isEmpty() ? QStringLiteral("HTTP %1").arg(status) : err);
            return;
        }
        b.done = true;
        b.ok = true;
        b.board = fb;
        job->icaos += seen;
        advanceJob(job);
    });
}

void FeedService::completeJob(const std::shared_ptr<PollJob> &job)
{
    const int idx = job->idx;
    m_poll[idx].busy = false;                    // before apply*: slots may re-poll

    if (!job->dep.ok && !job->arr.ok) {
        applyFailure(idx, !job->dep.err.isEmpty() ? job->dep.err : job->arr.err);
        return;
    }
    // Partial result: the board that failed (or was skipped anonymously)
    // stays unknown — never fabricated as empty.
    Sample s;
    s.stampUtc = QDateTime::currentDateTimeUtc();
    if (job->dep.ok)
        s.departures = job->dep.board;
    if (job->arr.ok)
        s.arrivals = job->arr.board;
    for (const QString &icao : std::as_const(job->icaos))
        airportSeen(icao);
    applySample(idx, s);
}

void FeedService::airportSeen(const QString &icao)
{
    const QString key = icao.trimmed().toUpper();
    if (!key.isEmpty())
        emit airportReported(key);
}

void FeedService::requestAirportFill()
{
    // Stub: OpenSky exposes no airport-list endpoint we use, so no fill can
    // run and none is reported as done.
}

void FeedService::completeAirportFill()
{
    emit airportFillCompleted();
}

bool FeedService::rateLimitPaused() const
{
    return m_blockedUntilMs > QDateTime::currentMSecsSinceEpoch();
}

void FeedService::syncPauseState()
{
    const bool paused = rateLimitPaused();
    if (paused) {
        const qint64 left = m_blockedUntilMs - QDateTime::currentMSecsSinceEpoch();
        m_pauseTimer.start(int(qMin<qint64>(left, 3600 * 1000)) + 20);
    } else {
        m_pauseTimer.stop();
    }
    if (paused != m_pauseReported) {
        m_pauseReported = paused;
        emit rateLimitPausedChanged(paused);
    }
}

void FeedService::applySample(int idx, const Sample &s)
{
    if (idx < 0 || idx >= siteCount())
        return;
    SiteData &d = m_data[idx];
    d.current = s;
    if (!s.stampUtc.isValid())
        d.current.stampUtc = QDateTime::currentDateTimeUtc();
    d.recent.push_back(d.current);
    if (d.recent.size() > SiteData::kHistoryCap)
        d.recent.remove(0, d.recent.size() - SiteData::kHistoryCap);
    d.error.clear();
    d.lastSyncUtc = QDateTime::currentDateTimeUtc();
    if (s.arrivals.known || s.departures.known)
        d.boardsFetchedUtc = d.lastSyncUtc;   // only a measured board counts
    d.state = SiteData::State::Live;
    if (!m_linkOnline) {
        m_linkOnline = true;
        emit linkStatusChanged(true);
    }
    finishPoll(idx);
}

void FeedService::applyFailure(int idx, const QString &error)
{
    if (idx < 0 || idx >= siteCount())
        return;
    SiteData &d = m_data[idx];
    d.error = error;
    // Keep the last good sample; just flag it stale.
    if (d.state == SiteData::State::Live)
        d.state = SiteData::State::Stale;
    else if (d.state == SiteData::State::Pending)
        d.state = SiteData::State::Error;
    if (m_linkOnline) {
        m_linkOnline = false;
        emit linkStatusChanged(false);
    }
    finishPoll(idx);
}

void FeedService::finishPoll(int idx)
{
    emit siteUpdated(idx);

    if (m_scanQueueRemaining > 0) {
        --m_scanQueueRemaining;
        ++m_scanDone;
        if (m_scanQueueRemaining == 0) {
            // "Last sync" must be a real claim: only stamp the sweep when at
            // least one site actually reported during it.
            for (const SiteData &d : std::as_const(m_data)) {
                if (d.state == SiteData::State::Live) {
                    m_lastSync = QDateTime::currentDateTimeUtc();
                    break;
                }
            }
        }
        emit scanProgress(m_scanDone, m_scanTotal);
    }
}
