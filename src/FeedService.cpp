#include "FeedService.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <utility>

FeedService::FeedService(const QVector<Site> &sites, QObject *parent)
    : QObject(parent)
{
    m_data.reserve(sites.size());
    for (const Site &s : sites)
        m_data.push_back(SiteData{s, {}, {}, SiteData::State::Pending, {}, {}});

    m_net.setTransferTimeout(9000);   // seam transport inherits a sane timeout
    connect(&m_scanTimer, &QTimer::timeout, this, &FeedService::advanceScan);
}

void FeedService::setSites(const QVector<Site> &sites)
{
    ++m_generation;                    // retire replies still in flight for the old roster
    m_scanTimer.stop();
    m_scanQueueRemaining = 0;
    m_scanCursor = 0;

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

void FeedService::pollSite(int idx)
{
    if (idx < 0 || idx >= siteCount())
        return;
    const Site &st = m_data[idx].site;

    // =========================================================================
    // FEED SEAM — replace this block with your data source.
    //
    // The sweep machinery (staggered cadence, queue bookkeeping, history,
    // alert fan-out) is already wired; your only job is turning a Site into
    // a Sample. The reference HTTP pattern:
    //
    //     QUrl url(st.endpoint);
    //     QNetworkReply *reply = m_net.get(QNetworkRequest(url));
    //     connect(reply, &QNetworkReply::finished, this,
    //             [this, idx, gen = m_generation, reply] {
    //         reply->deleteLater();
    //         // Roster swapped while this reply was in flight: the index no
    //         // longer names the site that was polled — drop the sample.
    //         if (gen != m_generation || idx < 0 || idx >= m_data.size())
    //             return;
    //         if (reply->error() != QNetworkReply::NoError) {
    //             applyFailure(idx, reply->errorString());
    //             return;
    //         }
    //         const Sample s = parseYourJson(reply->readAll());  // your code
    //         applySample(idx, s);
    //     });
    //
    // applySample()/applyFailure() also complete the poll's scan-queue
    // bookkeeping, so a wired poll is literally: parse → apply → done.
    // Never invent a value: leave fields NaN/-1 when the feed doesn't carry
    // them. (Full worked example: docs/recipes.md "Wire a feed".)
    // =========================================================================
    Q_UNUSED(st);

    // Stub: no feed wired, so nothing is invented. The poll completes empty
    // — sites stay Pending and render their explicit gap state — and the
    // sweep still advances so scan progress, the countdown and the log
    // behave exactly as they will once a feed is wired.
    finishPoll(idx);
}

void FeedService::airportSeen(const QString &icao)
{
    const QString key = icao.trimmed().toUpper();
    if (!key.isEmpty())
        emit airportReported(key);
}

void FeedService::requestAirportFill()
{
    // Stub: no feed client exists yet, so no fill can run and none is
    // reported as done. Replace with the real fill kick-off.
}

void FeedService::completeAirportFill()
{
    emit airportFillCompleted();
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
