# Changelog

## 1.7.0 — 2026-08-20

### Added
- **Complete Illustrious 18** in `CountingAgent` (pair-split 10,10 vs 5/6 skipped — the C++ engine has no split). Insurance stays take at TC ≥ +3.
- **Fab Four** surrender deviations when `Surrender` is legal: 14 vs 10 (≥ +3), 15 vs 9 (≥ +2), 15 vs 10 (≥ 0), 16 vs 9 (≥ −1).
- Shared `IndexPlay` table: `CountingAgent::indexPlays()` / `exportIndexPlays()`. `decide()` loops that table so tests and the CLI cannot drift.
- CLI **`blackjack indices`** (alias `index`) — text table by default, `--json` dumps the array.
- Pages table: **index-aware hints** (`countAction`; shows `stand · index` when it differs from basic), off-book counter vs the counting play, **H17 checkbox** on Play and the EV lab (default S17), **Indices** tab (table + drill), last-12 W/L/P `#history`.
- Web engine: `indexAction`, `countAction`, `h17DealerDone`, `INDEX_PLAYS`.

### Fixed
- 16 vs 9 stand index is TC ≥ **5** (was 4).
- 13 vs 2 hits only when TC **< −1** (stand at −1, matching I18).

### Changed
- Version string `kVersion` → `1.7.0`

### Documented
- **DAS** (double after split except aces) was already enabled on the Pages table.

## 1.6.0 — 2026-08-20

### Added
- **Wonging** — the counting agent wagers 0 when the true count is ≤ −1 (hand still dealt so the count moves).
- **`ev-grid`** — stand EV table for hard 5–20 vs 2–A (`--json` supported).
- Pages table: **pair split** (one split; aces get one card), **true-count bet spread**, session W/L/P, off-book counter, shoe penetration bar.

### Changed
- Version string `kVersion` → `1.6.0`

## 1.5.0 — 2026-08-20

### Added
- **GitHub Pages table** in `docs/` — play a 6-deck S17 game in the browser (hit/stand/double/surrender/insurance), Hi-Lo with a hidden hole card, basic-strategy hints, infinite-deck EV lab, and a count drill.
- **JSON strategy export** — `blackjack export-chart basic --format json`.
- Node tests for the web engine (`tests/web_engine.test.js`).

### Changed
- Version string `kVersion` → `1.5.0`

## 1.4.0 — 2026-08-20

### Added
- **Insurance** when the dealer shows an Ace (`Rules::allowInsurance`). US peek: taking or declining insurance looks at the hole card for blackjack; even money on a player natural locks in +1. The counter takes insurance at true count ≥ +3.
- **Infinite-deck EV table** — `blackjack ev --player 16 --dealer 10 [--soft]` prints stand / hit / double / surrender values.
- **Bankroll path** — `blackjack ruin --bankroll 200 --hands 20000` runs the counting agent until the units run out (or the hand cap).
- CLI rule flags: `--no-surrender`, `--no-insurance`, `--penetration F`.

### Changed
- Version string `kVersion` → `1.4.0`
- `evaluate` / `evaluateCounting` resolve the insurance offer before play so Ace-up dealer blackjacks are not played as live hands.

## 1.3.0 — 2026-08-20

### Fixed
- **Hole card counting** — the dealer's face-down card is no longer added to the Hi-Lo running count until it is turned up (naturals, busts, and dealer play-out). Index plays now use the post-deal true count; the bet still uses the pre-deal count.

### Added
- **Late surrender** — `Action::Surrender` (−0.5 units) on the opening two-card decision when `Rules::allowSurrender` is set (default on). Basic strategy surrenders hard 16 vs 9/10/A and hard 15 vs 10.
- **Edge standard error** — `Stats::edgeStderr()` and `edge_stderr` in `--json` output.
- `Deck::deal(bool counted)` / `Deck::count` / `Deck::hiLoValue` for correct unseen-card handling.
- Interactive play accepts `r` to surrender.

### Changed
- Version string `kVersion` → `1.3.0`
- Tabular Q-rows are 4-wide (Stand/Hit/Double/Surrender). Loader still accepts 1.2 3-value policy files.
- Rules line prints surrender on/off.

## 1.2.0

### Added
- **SARSA agent** (`SarsaAgent`) — on-policy TD control, CLI name `sarsa`
- **Expected SARSA agent** (`ExpectedSarsaAgent`) — lower-variance on-policy TD
  via ε-greedy expectation bootstrap; CLI names `esarsa` / `expected-sarsa`
- **HTML strategy chart export** — `export-chart --format html` writes a
  self-contained, colour-coded HTML table (inline CSS)
- **JSON CLI mode** — global `--json` on `train`, `eval`, `compare`, `demo`,
  and `version` for machine-readable summaries
- **Training progress** — `--progress` (every 10%) or `--progress-every N`
  prints episode progress to stderr during self-play
- **Parallel evaluation** in `compare` / `demo` via `std::thread` (each thread
  owns its own `Environment`; agents are read-only during scoring)
- Unit tests for SARSA, Expected SARSA, HTML export, and learning-beats-random
  coverage for the new agents

### Changed
- Version string `kVersion` → `1.2.0`
- Interactive menu lists SARSA and Expected SARSA training options
- README / DESIGN docs updated for the new agents and CLI flags

### Not in this release
- Split hands and insurance remain out of scope (see DESIGN.md simplifications);
  prioritised solid TD agents + export/CLI over a half-broken ruleset expansion
