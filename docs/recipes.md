# Recipes

Step-by-step extension guides. Each names the exact files and seams — the
architecture stays one-directional (`docs/architecture.md`), so every
recipe touches the same few places.

---

## 1. Wire a feed

`FeedService::pollSite()` is already wired to OpenSky (see
`docs/architecture.md`). The recipe below is the generic pattern for wiring a
different source: unmeasured values render explicit gaps ("AWAITING FIRST
SCAN") — the missing-data convention working as designed.

**Edit `src/FeedService.cpp` → `pollSite()`** — replace the OpenSky block:

```cpp
void FeedService::pollSite(int idx)
{
    if (idx < 0 || idx >= siteCount())
        return;
    const Site &st = m_data[idx].site;

    QNetworkReply *reply = m_net.get(QNetworkRequest(QUrl(st.endpoint)));
    connect(reply, &QNetworkReply::finished, this,
            [this, idx, gen = m_generation, reply] {
        reply->deleteLater();
        // Roster swapped while this reply was in flight: the index no
        // longer names the site that was polled — drop the sample.
        if (gen != m_generation || idx < 0 || idx >= int(m_data.size()))
            return;
        if (reply->error() != QNetworkReply::NoError) {
            applyFailure(idx, reply->errorString());
            return;
        }
        Sample s = parseBody(reply->readAll());     // your function, below
        applySample(idx, s);
    });
}
```

That's the whole wiring: `applySample()` marks the site Live, stamps the
sync clock, appends to the capped history, flips the link flag and runs
the scan-queue bookkeeping; `applyFailure()` degrades to Stale/Error and
keeps the last good sample. Cards, chart, alert rules and the event log
light up with no further changes.

**Parsing** — same file, static helper. Keep the convention: fields the
payload doesn't carry stay NaN/-1, never zero:

```cpp
static Sample parseBody(const QByteArray &body)
{
    Sample s;
    s.stampUtc = QDateTime::currentDateTimeUtc();
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    if (o.contains("load"))        s.loadPct   = o.value("load").toDouble(std::nan(""));
    if (o.contains("latency_ms"))  s.latencyMs = o.value("latency_ms").toDouble(std::nan(""));
    if (o.contains("sessions"))    s.sessions  = o.value("sessions").toInt(-1);
    return s;
}
```

Notes:

- `m_net` is already constructed with a 9 s transfer timeout.
- The `gen = m_generation` capture is the generation-counter guard —
  keep it for any async lookup you ever write in this codebase
  (docs/architecture.md).
- The roster is editable at runtime; users paste per-site URLs into
  SITE ADMIN's ENDPOINT field (`Site::endpoint`), persisted in `sites.json`.
- To poll something that isn't HTTP (serial, MQTT, a subprocess), the seam
  is the same: schedule/answer however you like, then call
  `applySample`/`applyFailure`.

### Airport hooks

If your feed names airports, call `airportSeen(icao)` for each ICAO in the
reply (before or after `applySample`); unknown ones are learned into
`airports.json` with the ICAO only. Implement `requestAirportFill()` to
start a directory fill and call `completeAirportFill()` when it succeeds
(only then is the damaged-file refill flag cleared). On failure, don't
call it — the flag stays set and the next start retries.

---

## 2. Add a data domain (a metric)

Example: an `errorsPct` metric, claimed color Cyan (spare accent).

1. **Model** — `src/SiteData.h`: add `double errorsPct = std::nan("");` to
   `Sample`. Doubles start NaN, ints start -1. That's the whole convention.
2. **Feed** — `FeedService.cpp::parseBody`: populate it like any field,
   leaving NaN when absent.
3. **Card** — `src/SiteCard.cpp`: add a labeled block to `paintEvent`
   (label, formatted value, `Theme::Cyan`) in free space between the
   collision zones. Respect the collision zones in
   docs/design-system.md and re-measure with a `--grab`.
4. **Rules** — a threshold in `evaluateAlerts` (next recipe).
5. **Theme** — if the domain needs a ramp (value-scaled color), write a
   sibling of `Theme::loadColor()`; do not fork existing values.

The one-accent-per-domain rule (docs/design-system.md) is the only design
constraint: pick an unclaimed accent and use it everywhere the metric
appears.

---

## 3. Add a watch rule

Rules live in `evaluateAlerts()` (`src/SiteData.h`) — pure functions of one
`SiteData`, no side effects, no logging (MainWindow diffs and logs).

```cpp
if (!std::isnan(c.errorsPct)) {
    if (c.errorsPct >= 10)     add(Alert::Sev::Crit, "ERRORS", QStringLiteral("ERROR RATE %1%").arg(c.errorsPct, 0, 'f', 1));
    else if (c.errorsPct >= 3) add(Alert::Sev::Warn, "ERRORS", QStringLiteral("ERROR RATE %1%").arg(c.errorsPct, 0, 'f', 1));
}
```

Rules of the road:

- **Keys are stable rule ids.** The event log diffs raise / severity-change
  ("raised to CRITICAL" / "lowered to WARNING") / clear ("…nominal") on the
  key alone. A changing value in the key would re-fire every sweep; put
  live values in the message only.
- Guard every field (`isnan` / `< 0`) — a rule must not fire on missing
  data.
- Same key at two severities = one rule that escalates; different keys =
  independent rules that can coexist.

**Trend rules** read the history window: see `LOADRISE` in the shipped
rules and `SiteData::loadTrend3()` for the delta-over-last-N pattern.

**Derived-rule classes** (optional): rules computed ahead of the current
sample — "latency will exceed 400 ms within N samples" — belong in a
separate `evaluateForecastAlerts(d)`-style function returning keys prefixed
`FC_` at `Sev::Info`. Diff them in `MainWindow::runAlertRules` on the key
only (a changing lead time must not re-log) and log them at their own
level — the `FCST` log level already renders magenta in `EventLog`. The
extracted origin app shipped exactly this pattern; keep its diffing
separate from severity-based rules.

---

## 4. Rename the project

```bash
scripts/rename-project.sh FleetConsole
```

Rewrites the identity tokens across every tracked file (CMake project and
target, window title, log source, packaging artifacts, `.desktop` file,
icon name, Wayland app id, Docker image names), renames
`packaging/linux/<id>.desktop`, then rebuilds and runs an offscreen grab so
a broken rename never lands silently (`--no-verify` skips). Do this before
your first commit; the repo name on GitHub is unaffected — set it from the
GitHub side.

---

## 5. Add a card instrument

`SiteCard::paintEvent` is plain `QPainter` with Theme primitives. Ground
rules:

- **Calm UI**: card content must be static between data updates and the
  minute tick — anything animated belongs in the window header.
- Measure the zone you claim before and after (bright-pixel spans, not
  eyeballing — docs/design-system.md "Layout QA"); the default card is
  384×288 and stretches wider in the grid.
- Missing data renders a gap (`---`, dashed outline — see
  `GlyphPaint::draw(Glyph::None)`), never zero.
- Icons: paste verbatim Bootstrap Icons 1.11.3 `d` data into
  `src/widgets/IconGlyph.h` with a new enum entry; never freehand geometry.

## Flight boards on `Sample`

`Sample::arrivals` / `Sample::departures` are `FlightBoard`s with a `known`
flag. `known=false` (the default) means unmeasured or failed — render gaps,
never zero. `known=true` with no flights is a real empty board. Fill them with
`parseBoard(httpStatus, body, &board, &err, &icaos)` from `OpenSkyParse.h`
(HTTP 404 with body `[]` is a valid empty board), then call
`airportSeen()` for each ICAO in `icaos`.

## OpenSky credentials

`OpenSkyAuth` (`src/OpenSkyAuth.h`) loads OpenSky API client credentials:

1. Environment (wins): `OPENSKY_CLIENT_ID` and `OPENSKY_CLIENT_SECRET` (both required).
2. Otherwise `opensky.json` in the app config dir
   (`QStandardPaths::AppConfigLocation`, next to `sites.json`):
   `{"client_id": "...", "client_secret": "..."}`.

With neither, it runs anonymous: `withToken(cb)` calls `cb("", "")`. With
credentials it performs the OAuth2 client-credentials grant, caches the token
in memory, refreshes it 30 s before expiry and coalesces concurrent callers
onto one fetch. On failure `cb("", error)` where `error` names the HTTP status
only. The id, secret and token are never logged or put in error strings —
keep it that way in callers.
