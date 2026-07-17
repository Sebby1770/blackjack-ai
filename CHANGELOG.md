# Changelog

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
