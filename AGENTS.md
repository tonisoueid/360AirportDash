# AGENTS.md — dashboard-template-qt-cpp

Operational guide for AI coding agents working in this repository (Claude
Code, Codex, Cursor and other agent tools read this file natively). Update
it when build steps or architecture change.

## Project

Qt 6 / C++20 desktop template for NOC-style telemetry dashboards: an
editable roster of watched sites, a staggered polling service, watch rules
that log raise and clear, and a fully custom-painted dark console
(charcoal-blue chassis, one saturated accent per data domain, no bitmap
assets). One executable, `360AirportDash`.

**Feed wired to OpenSky**: `FeedService::pollSite()` fetches departures
(and, with OpenSky credentials, arrivals) per site ICAO; with no credentials
it polls departures only and arrivals stay a gap. `--offline` makes no calls.
Failed or unmeasured boards render explicit gaps (docs/recipes.md "Wire a
feed" covers the seam pattern). HTTP 429 honours `Retry-After` and pauses all
OpenSky requests; 5xx is retried once.

No tests are bundled; no linter is configured. Verification is headless
render + measurement (below) — preserve those modes.

## Build & run (Linux)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/360AirportDash
```

Requires Qt 6 (Widgets + Network dev packages), CMake ≥ 3.21.

- New source files **must** be added to the explicit `add_executable` list
  in `CMakeLists.txt` (no globbing). The packaging scripts pick up new
  files automatically once CMakeLists lists them — nothing else to update.
- `CMAKE_AUTOMOC` is on: `Q_OBJECT` classes need no extra wiring.
- `if(WIN32)` sets `WIN32_EXECUTABLE` (no console window on Windows) — do
  not remove.

## Verifying without a GUI

Built-in headless modes; use them for any visual/behavioral check:

- `--offline` — link-down drill, no polling (also disables the `R` rescan key)
- `--grab frame.png` — render one frame after the first scan completes
  (30 s fallback exit — give `--offline --grab` runs an outer timeout ≥ 45 s),
  then exit
- `--grab-admin <prefix>` — scripted admin-dialog add + save, screenshots to `<prefix>-*.png`
- `--icon FILE` — render the 256 px app icon, then exit

Always `QT_QPA_PLATFORM=offscreen` for headless runs. For data-path
testing, write an ad-hoc CMake harness in `/tmp` that links the real
widgets from `src/` and drives them directly (e.g.
`FeedService::applySample()` with synthetic samples). Host widgets in an
opaque window before grabbing — a bare `widget->grab()` shows alpha-garbage
at transparent edges.

**Visual QA honesty rule:** AI judgments of "centered/aligned" are
unreliable — measure bright-pixel spans per region instead of eyeballing.
Card background is `#121820` vs panel `#11161d` (≤ 3 per channel apart —
matching tolerance must be < 3); content pixels have a channel > 70
(exclude BorderHi borders explicitly). Collision zones are listed in
docs/design-system.md.

## Packaging

| Target | Command | Artifact |
|---|---|---|
| Windows 10/11 x64 | `packaging/windows/build-win64.sh` | `dist/360AirportDash-0.1.0-win64.zip` |
| Linux x86_64 | `packaging/linux/build-linux.sh` | `dist/360AirportDash-0.1.0-linux-x86_64.tar.gz` |
| Linux (dev) | the plain cmake build above | `build/360AirportDash` |

Both scripts run in Docker (Fedora 43 base; images build on first use) and
need host Qt 6 Gui + CMake + Ninja + python3 for icon rendering
(`packaging/common/gen-icons.sh`, from `src/AppIcon.h`). Zero source
changes needed — if a change breaks cross-compilation, the change is wrong,
not the scripts. The Linux archive is smoke-tested (`--offline --grab`,
offscreen) in the container before it is written. macOS is not a target.

## Architecture

Data flows one way: `SiteRegistry` (roster) → `FeedService` (polling) →
`MainWindow` (alert diffing, wiring) → `SiteCard` (one per airport) / `TrendChart` / `EventLog`
(painting). Details and rationale: **docs/architecture.md** (patterns: the
feed seam, generation counter, stale semantics, alert diffing, history
window). Short version:

- **`src/SiteData.h`** — shared header: data model, default roster, watch
  rules (`evaluateAlerts`). To add or tune an alert, edit it here.
- **`src/SiteCard`** — per-airport card: ICAO, status badge
  (`cardBadge()`), city/country text, local clock (IANA zone; none if
  empty/invalid), arrivals/departures (`---` when the board is unknown),
  "updated N min ago" (none without a fetch). `MainWindow::refreshCard()`
  feeds it data, pause state and `AirportRegistry::find()` metadata; clock
  and age repaint only on the minute gate.
- **`src/widgets/FlightTable`** — custom-painted sortable/filterable/virtualized
  table for one `FlightBoard` (`setBoard(ptr, mode)`, `setFilter`, `refreshTick`);
  model rebuilt on change, paints visible rows only, explicit gap vs empty states.
- **`src/SiteRegistry`** — roster persisted as `sites.json` (QSaveFile,
  corrupt → defaults). **Site indexes change after a swap
  (`sitesReconfigured`) — never cache indexes across it.**
- **`src/FeedService`** — staggered polling, one in-flight request per
  site, `kStaggerMs = 450` apart, sweep every `kRefreshSecs = 900`. To
  change the sweep interval edit **only** `kRefreshSecs`. The FEED SEAM
  (OpenSky `pollSite` + `applySample`/`applyFailure`) is the template's
  central extension point.
- **`src/MainWindow`** — owns timers; diffs alert results so the event log
  records raise, severity change and clear ("…nominal"). Hotkeys in
  `keyPressEvent`: `R` rescan (disabled offline), `1`–`9` select site,
  arrows move selection (clamped, no wrap), `Ctrl+E` site admin, `Ctrl+Q`
  quit.
- **`src/AdminDialog`** — edits a copy of the roster; only
  `SiteRegistry` persists. The only stylesheet user — instrument
  surfaces stay custom-painted.
- **Rendering** — every instrument surface is custom `QPainter`; colors,
  fonts, per-domain accents live in `Theme.h`; icon artwork is verbatim
  Bootstrap Icons v1.11.3 SVG path data (MIT) through the parser in
  `widgets/IconGlyph.h` — never freehand icon geometry.

### Invariants — preserve these in every change

1. **Missing-data convention**: every measurement starts NaN (doubles) or
   -1 (ints); the UI renders explicit gaps (`---`, dashed baselines), never
   zero. Check `std::isnan` / `< 0` before using any value. "Measured,
   never invented" is the core promise — never fake a reading.
2. **UI claims must be wired**: any countdown/status promise (e.g. "Next
   scan", "Last sync") must have a backing code path that only reports what
   actually happened (a site that has not reported is never counted live and
   refuses to stamp "Sync" without a reporting site).
3. **Calm UI**: cards repaint on a minute gate and data updates; anything
   blinking belongs in the window header. New card elements must be static
   between minute ticks and data updates.
4. **Alert keys are stable rule ids** — live values belong in messages;
   the log diffs raise/severity-change/clear on the key alone.
5. **One accent per data domain** — see the design system before adding
   colors or fonts.

## Design system (owner-set rules — do not regress)

- Multi-accent palette on the `#0b0e13` chassis; exactly one saturated
  accent per data domain; severity colors fixed (Info blue, Warn amber,
  Crit red, clear green). Full tables: **docs/design-system.md**.
- **Legibility floor**: all text flows through `Theme::sans()/mono()`
  (×1.2 scaling, 10 px minimum). Never hardcode pixel font sizes outside
  `Theme.h`.
- Every scale/gauge carries labeled, meaningful numbers (units, watch-rule
  thresholds), not bare extremes.
- Card layout collision zones and the measurement method: see
  docs/design-system.md "Layout QA". Re-check after any font/layout change
  with an offscreen `--grab`.

## Documents

- `README.md` — outward-facing overview.
- `docs/architecture.md` — patterns and rationale (read before structural
  changes).
- `docs/design-system.md` — palette, typography, primitives, layout QA.
- `docs/packaging.md` — both targets, bundle policies, verification.
- `docs/recipes.md` — the extension how-tos (wire a feed, add a domain,
  add a watch rule, rename).
