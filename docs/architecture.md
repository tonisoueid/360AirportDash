# Architecture

A polled-telemetry operations console: an editable roster of watched sites,
a staggered polling service, a rule engine whose events log raise and clear,
and a fully custom-painted dark UI. One executable, no runtime dependencies
beyond Qt 6 (Widgets + Network).

## Data flow (one direction only)

```
SiteRegistry ──► FeedService ──► MainWindow ──► SiteCard / TrendChart / EventLog
   (roster)      (polling)       (wiring:
                                  timers, alert
                                  diffing, selection)
```

Nothing downstream reaches back up. `SiteRegistry` owns the roster and
persists it; `FeedService` turns roster entries into `SiteData` samples;
`MainWindow` wires signals to widgets and diffs watch rules; the widgets
only paint what they are handed.

## Source map

| File | Role |
|---|---|
| `src/OpenSkyAuth.h/.cpp` | OpenSky OAuth2 client-credentials token manager: env/config-file credentials, in-memory token refreshed ~30 s before expiry, coalesced fetches, anonymous mode without credentials. Never logs secrets. See docs/recipes.md "OpenSky credentials". |
| `src/OpenSkyParse.h/.cpp` | Pure OpenSky `/flights/arrival` / `/flights/departure` parser (`parseBoard`); 404 + `[]` is a known-empty board; reports ICAOs seen for `airportSeen()`. No networking. |
| `src/SiteData.h` | Data model (`Site`, `Sample`, `FlightBoard`, `SiteData`), default roster, watch rules (`evaluateAlerts`). Everything downstream follows its missing-data convention. |
| `src/SiteRegistry.{h,cpp}` | Editable roster persisted as `sites.json` in `QStandardPaths::AppConfigLocation`. `setSites()` normalizes → validates → persists (`QSaveFile`) → emits. |
| `src/FeedService.{h,cpp}` | Round-robin polling, one in-flight request per site, staggered `kStaggerMs` apart, full sweep every `kRefreshSecs`. **`pollSite()` is wired to OpenSky** (departures; arrivals too when `OpenSkyAuth` has credentials, otherwise a gap): sequential per-site requests, 429 `Retry-After` pause (global) and 5xx single retry; partial results leave the failed board unknown; every poll ends in one `applySample`/`applyFailure`. |
| `src/MainWindow.{h,cpp}` | Owns the timers; diffs `evaluateAlerts` between samples so the event log records raise, severity change and clear ("…nominal"). Hotkeys, selection, panel focus. |
| `src/SiteCard.{h,cpp}` | Per-airport card (ICAO, badge, city/country, local clock, arrivals/departures, updated-ago), custom `QPainter`; metadata arrives via `setAirport()`. |
| `src/widgets/TrendChart.{h,cpp}` | Dual-scale history trace for the selected site. |
| `src/widgets/FlightTable.{h,cpp}` | Custom-painted sortable/filterable/virtualized table for one `FlightBoard` (`setBoard(ptr, mode)`, `setFilter`, `refreshTick`). Row model, filter and sort are rebuilt on change only; `paintEvent` draws just the visible rows. Unknown board paints a dashed gap panel, known-but-empty paints "no flights in window"; no timers. |
| `src/EventLog.{h,cpp}` | Timestamped UTC log, capped at 300 lines. |
| `src/AdminDialog.{h,cpp}` | Roster editor (table + form). ICAO-only Add/Update, duplicate rejection, ENABLED/PINNED/poll-interval editing, IATA→ICAO hint (`iataToIcao()` in `src/AirportSearch`). Only stylesheet user. |
| `src/widgets/Panel`, `ExpandButton`, `BodySplitter` | Captioned card chrome, caption-corner expand, draggable body seam. |
| `src/widgets/IconGlyph.h` | Deterministic SVG-path parser + verbatim Bootstrap Icons artwork. |
| `src/Theme.h` | Palette, typography, drawing primitives — the design system. |
| `src/AppIcon.h` | Painted app icon (no bitmap assets anywhere). |

## The patterns worth keeping intact

### Missing-data convention

Every measurement starts NaN (doubles) or -1 (ints). The UI renders explicit
gaps — `---`, dashed baselines, "AWAITING FIRST SCAN" — and never zero.
Check `std::isnan` / `< 0` before using any value. A feed that omits a field
must leave it invalid; nothing downstream may invent a reading.

### Staggered sweep, degraded-not-dropped

`FeedService` walks the roster like a NOC polling telemetry stations:
`pollSite(0)`, 450 ms later `pollSite(1)`, …, then the sweep is done and the
next one is armed `kRefreshSecs` out. One request is in flight per site at
most. On failure the site degrades — Live → **Stale** (last good sample
retained, data age grows) or Pending → **Error** — and the header LINK chip
flips. Recovery is symmetric. To change the sweep interval edit **only**
`kRefreshSecs` (the status-bar countdown reads it).

### Generation counter (retires superseded replies)

The roster is hot-swappable at runtime. `setSites()` bumps `m_generation`;
every in-flight network reply captured the generation it was issued under
and drops itself if the number moved — the reply's site index no longer
names the site that was polled. The same pattern guards any async lookup
you add — search-as-you-type fields, secondary feeds, anything where a
reply can outlive the state that issued it. Consequences:

- Samples survive a roster edit when the site's `code` **and** `endpoint`
  are unchanged.
- **Site indexes change after a swap (`sitesReconfigured`) — never cache an
  index across it.**

### History window

`applySample()` appends to `SiteData::recent`, capped at `kHistoryCap`
(default 48). Everything historical — trend chart, trend
watch rules — reads that window; nothing else keeps its own buffer.

### Alert diffing (raise / severity-change / clear)

`evaluateAlerts()` is a pure function of one `SiteData`. `MainWindow`
stores the last result per site keyed by the rule's stable `key` and diffs:

- new key → log the raise,
- same key, different severity → log "raised to CRITICAL" / "lowered to
  WARNING",
- vanished key → log "<rule> nominal".

This is why alert **keys must be stable identifiers** and changing values
belong in the message, not the key. Derived/forecast-style rules
(early warnings computed from history) follow the same diffing but log at
their own level — see docs/recipes.md "Add a derived-rule class".

### Persistence

`SiteRegistry` writes `sites.json` (versioned object, `QSaveFile` so a
crash never truncates it) under `AppConfigLocation`
(`~/.config/<appname>/` on Linux, `%APPDATA%` on Windows). A corrupt or
invalid file falls back to the built-in defaults and logs a WARN — the
console always boots.

### OpenSky feed (`FeedService::pollSite`)

- Per site ICAO (`Site::code`) it requests `/flights/departure`, then — only
  with `OpenSkyAuth` credentials — `/flights/arrival`, over the trailing
  `kWindowSecs` (24 h). Requests for one site are sequential, so a site has
  at most one request or wait pending.
- **Anonymous mode** (no credentials): no auth header, departures only; the
  arrivals board stays `known=false` (a gap, never "empty").
- **Outcomes**: every poll ends in exactly one `applySample` / `applyFailure`
  (scan-queue bookkeeping). 404 + `[]` is a known-empty board. Other 4xx and
  transport errors fail the board; both boards failing → `applyFailure`
  (last good sample kept, state Stale/Error). One board ok + one failed →
  `applySample` with the failed board left unknown.
- **429**: `Retry-After` (or `X-Rate-Limit-Retry-After-Seconds`, default
  60 s, max 3600 s) sets a global pause on all OpenSky requests; the board is
  retried after the delay, failing after `kMax429Retries`. **5xx**: retried
  once, then the board fails.
- A roster swap bumps the generation counter and cancels the per-site
  timers; late replies are dropped. `--offline` never starts polling.

- **Pause signal**: `rateLimitPaused()` is true only while a real 429
  `Retry-After` pause is in force; `rateLimitPausedChanged(bool)` fires on
  the begin and lapse edges (a single-shot timer covers the lapse).
  `MainWindow` refreshes every card on it.
- **Boards stamp**: `SiteData::boardsFetchedUtc` is set in `applySample` only
  when at least one board is measured (`known`); a failure never touches it.
- **Board window**: each measured `FlightBoard` carries `windowBeginUtc` /
  `windowEndUtc`, the begin/end actually sent for that board (departures and
  arrivals separately). Both are invalid for an unknown board or a
  hand-built sample — never invented. `boardWindowLabel()` renders
  "hour ending 09:15Z" / "24 h ending 09:15Z" from them (empty if unknown).
- **Row helpers** (`SiteData.h`): `cellCallsign/Icao24/From/To/FirstSeen/
  LastSeen` return "—" for any empty/invalid field; `flightStatus()` derives
  its string only from `firstSeen`/`lastSeen` as reported ("—" if absent or
  inconsistent).
- **`refreshSite(int idx)`**: starts a real re-poll of one airport; returns
  false (no-op) for a bad index, a poll already in flight, or an active 429
  pause. It is flagged manual so it never counts toward scan progress.
  Callers must not invoke it under `--offline`.

### Card badge state (`cardBadge(SiteData, paused)`)

Derived, never stored. Evaluated in this order:

1. `Paused` — pause active and the site is not `Live` (no fresh data).
2. `Stale` / `Error` — the matching `SiteData::State`.
3. `Pending` — never polled (also `Live` with no known board: a gap, not a claim).
4. `Live` — at least one *known* board holds flights.
5. `Quiet` — at least one known board, and every known board is empty.

`known=false` boards are skipped, never counted as zero flights.

### Airport card

`MainWindow::refreshCard(i)` pushes `SiteData`, `rateLimitPaused()` and the
`Airport` found by `AirportRegistry::find(site.code)` (case-insensitive;
no match → gaps) into `SiteCard`, on site updates, pause edges and
`airportsChanged`. The card draws the `cardBadge()` badge, city, country
text (no flags), a clock only when `Airport::timeZone` is a valid
`QTimeZone`, `---` for any `known=false` board, and the "updated" stamp only
when `boardsFetchedUtc` is valid. Clock and age are read at paint time and
repaint only on the minute gate. `TrendChart` is unaffected (it still reads
the history); the old load gauge/sparkline were removed from the card.

### Airport learning (feed hook)

`AirportRegistry` (airports.json, seeded from the compiled-in table) is
owned by `MainWindow` and driven only by what the feed reports:

- `FeedService::airportSeen(icao)` — call it whenever a sample or state
  names an ICAO. It normalises and emits `airportReported`; `MainWindow`
  calls `AirportRegistry::addLearned()` (a no-op for known ICAOs).
- After a corrupt `airports.json`, `needsRefill()` stays true (saving a
  learned entry does not clear it). `MainWindow` calls
  `FeedService::requestAirportFill()` at startup (not offline); the real
  client calls `completeAirportFill()` when done, which emits
  `airportFillCompleted` and `MainWindow` then calls `clearRefill()`.
- `pollSite()` calls `airportSeen()` for every ICAO named in the boards, so
  airports are learned. `requestAirportFill()` is still a stub (no airport
  list endpoint is used), so the refill flag is not cleared by the feed
  (invariant 2).

### Calm UI

Cards repaint on data updates and a minute tick, not per second; the only
things that move at 1 Hz or faster live in the window header (UTC clock,
blinking LIVE dot, scan counter). New card elements must stay static
between minute ticks and data updates.

## Verifying without a GUI

Built-in headless modes (always `QT_QPA_PLATFORM=offscreen`):

- `--offline` — link-down drill, no polling, `R` disabled
- `--grab FILE` — render one frame after the first scan completes
  (30 s fallback exit; give `--offline --grab` runs an outer timeout ≥ 45 s)
- `--grab-admin PREFIX` — scripted admin-dialog add (ICAO, pinned, 600 s poll) + save, screenshots to
  `PREFIX-*.png`
- `--icon FILE` — render the 256 px app icon

For interaction or data-path testing, write an ad-hoc CMake harness in
`/tmp` that links the real widgets from `src/`, drives them directly (e.g.
`FeedService::applySample()` with synthetic samples) and grabs full windows
— a bare `widget->grab()` shows alpha-garbage at transparent edges, so host
the widget in an opaque window first. This repo was validated exactly that
way; keep the habit.
