# Changelog

## 1.2.1

### Fixed
- **The Hi-Lo count included the dealer's hole card before it was ever shown.**
  `Deck::deal()` added every card to the running count the moment it left the
  shoe, including the face-down hole card. When the player busts the dealer
  scoops the bet without exposing that card, so the simulated counter was
  using information a real counter never sees. Measured over 40 player-bust
  hands, 29 had a polluted count (the rest drew hole cards tagged 0, where
  counting is a no-op).

  Face-down cards are now dealt with `Deck::dealHidden()` and only enter the
  count via `Deck::reveal()`, called when the dealer actually turns the card
  over — on a natural, or before playing the hand out. Play and betting
  decisions are unaffected in shape; the counting agent's simulated edge drops
  (roughly halved across four 200k-hand seeds), which is the point: the old
  number was optimistic because it leaked information.

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
