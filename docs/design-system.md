# Design system

The rules below are the template's contract: they keep every derived
dashboard looking like one coherent instrument console. `src/Theme.h` is
the single source of truth; do not fork these values into widgets.

## Palette

Chassis (charcoal-blue, darkest behind the log):

| Role | Hex | Used for |
|---|---|---|
| `Bg` | `#0b0e13` | window base |
| `BgDeep` | `#080a0f` | log well, chart chip backs |
| `PanelBg` / `PanelBgHi` | `#11161d` / `#171d26` | panel fill / inputs, hover |
| `CardBg` | `#121820` | site-card fill |
| `Border` / `BorderHi` | `#252c36` / `#3c4654` | hairlines / hover + ticks |
| `Grid` / `GridSoft` | `#26303c` / `#212a36` | chart graticule / empty gauge segments |

Text — four contrast tiers, all deliberately bright on the chassis (the
dim tiers were tuned brighter on purpose; do not "fix" them darker):

| Role | Hex |
|---|---|
| `Text` | `#ecf2f8` |
| `TextSoft` | `#d0dae4` |
| `TextMuted` | `#a9b5c2` |
| `TextFaint` | `#8a96a3` |

## One saturated accent per data domain

The palette's core idea: exactly one saturated accent per data domain, so
color always means the same thing across cards, charts, chips and the log.
The shipped example maps:

| Accent | Hex | Domain (example) |
|---|---|---|
| `Orange` / `OrangeBright` | `#f0883e` / `#ffa657` | primary metric — load |
| `Blue` / `BlueBright` | `#58a6ff` / `#79c0ff` | latency, network |
| `Green` / `GreenBright` | `#3fb950` / `#56d364` | status / OK |
| `Amber` | `#e3b341` | warnings |
| `Red` / `RedBright` | `#f85149` / `#ff7b72` | critical |
| `Magenta` | `#f06ec0` | derived early-warning rules (FCST level) |
| `Cyan`, `Purple` | `#56d4dd`, `#b48cea` | spare — claim them for domains you add |

When you add a metric, pick one accent and use it *everywhere that metric
appears* (card row, chart series, legend, chip). Never reuse an accent for
a second domain, and never return to a monochrome palette.

Severity colors are fixed: Info → Blue, Warn → Amber, Crit → Red, clear
events → Green.

## Typography

All text flows through `Theme::sans()` / `Theme::mono()`:

```cpp
f.setPixelSize(qMax(10, int(px * 1.2 + 0.5)));
```

- Global legibility floor: nothing renders below 10 px, everything scales
  ×1.2. **Never hardcode a pixel font size outside `Theme.h`.**
- `spaced = true` adds letter-spacing (labels like "AWAITING FIRST SCAN").
- Numeric readouts use the same system font (`mono` is an alias) — the
  console look comes from size and color tiers, not a monospace family.

## Drawing primitives

- `Theme::panelBody(painter, rect, radius, fill, border)` — every card
  chassis: rounded fill + hairline border.
- `Theme::chip(painter, rect, text, color, font)` — outlined pill with a
  13 % tinted fill; the tag style for statuses and categories.
- `Theme::gaugeRamp(i, n)` — cold-blue → hot-red interpolation for
  segmented gauges; `Theme::loadColor(pct)` — the example 0–100 domain ramp
  (`Green ≤ 40 · Amber ≤ 60 · Orange ≤ 80 · Red`).

Numbers on gauges and scales must be **labeled and meaningful** — units,
watch-rule thresholds (the load gauge ticks 100/75/50 sit on the WARN
thresholds), never bare extremes.

## Icon artwork

All glyphs are verbatim Bootstrap Icons v1.11.3 SVG path data, converted at
runtime by the deterministic parser in `src/widgets/IconGlyph.h`. Never
freehand icon geometry — pull the pinned-version `d` attribute from the
upstream repo and paste it verbatim (MIT, see LICENSE + README credit).

## Layout QA (the honest way)

AI/human eyeball judgments of "centered/aligned" are unreliable — measure
bright-pixel spans instead:

- Card background `#121820` vs panel `#11161d` differ by ≤ 3 per channel —
  a matching tolerance **below 3** is required to segment them.
- Content pixels: any channel > 70 (text tiers start at `#8a`; chassis and
  grid colors stay ≤ 84 for BorderHi — exclude it explicitly where it
  borders a zone).
- Grab frames with `--grab`; for harness-driven widgets, host them in an
  opaque window first — bare `widget->grab()` shows alpha-garbage at
  transparent edges.
- Airport-card collision zones (default 384×288, cards stretch wider in the
  grid; content spans x = 16…width−16). Measured bright-pixel bands:
  ICAO + status badge y≈15–41 (ICAO left, badge chip right-aligned);
  city y≈56–69; country y≈80–89; hairline divider y=104 (excluded, ≤ BorderHi);
  local clock label y≈117–125 and time + zone abbreviation y≈136–157
  (absent entirely when the IANA zone is empty/invalid); ARRIVALS / DEPARTURES
  labels y≈187–195 and counts y≈208–229 (departures column starts at
  x = width/2); "UPDATED N MIN AGO" y≈265–273 (absent with no fetch).
  Missing city/country render `---` in `TextMuted`. Badge colors: LIVE green,
  QUIET Info blue, STALE and PAUSED Warn amber, ERROR Crit red; Pending is a
  dashed `---` chip, not a status. Re-measure after any font or layout change.
