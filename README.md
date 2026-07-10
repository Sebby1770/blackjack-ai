# Blackjack with AI 🃏

A Blackjack engine in modern **C++17** whose players are driven by agents that
learn how to play entirely from self-play — no human strategy is hard-coded into
them — plus a **Hi-Lo card counter** that turns the house edge positive. Five
approaches are implemented and benchmarked against textbook basic strategy and a
random baseline:

- **Q-Learning** — off-policy temporal-difference control
- **Double Q-Learning** — twin Q-tables that reduce overestimation bias
- **Monte Carlo control** — on-policy, first-visit, learning from full-hand returns
- **Card counting** — Hi-Lo running/true count with a bet spread and index-play deviations
- **Basic strategy** — the hand-coded, near-optimal benchmark

Every player and the dealer hold a `Hand` of `Card`s, and each agent decides what
to do based on the cards on the table. The project explores how an agent can
*discover* near-optimal Blackjack play through simulated trial and error — and how
counting cards tips the game in the player's favour.

**Version 1.1.0**

[![CI](https://github.com/Sebby1770/blackjack-ai/actions/workflows/ci.yml/badge.svg)](https://github.com/Sebby1770/blackjack-ai/actions/workflows/ci.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![License: MIT](https://img.shields.io/badge/license-MIT-green)

---

## Results

Agents are trained from scratch by self-play, then evaluated on **10,000,000
fresh hands** (6-deck, dealer stands on soft 17, blackjack pays 3:2).
`edge/hand` is the average betting units won per hand — closer to `0` is better,
and **positive means the player is beating the house**. Reproduce with
`./blackjack compare --episodes 1000000 --hands 10000000 --seed 2024`.

| Agent            | Win %  |   Bankroll | Edge / hand | Avg bet | Notes                               |
| ---------------- | :----: | ---------: | :---------: | :-----: | ----------------------------------- |
| Random           | 30.2 % | −4,413,356 |  **−0.441** |  1.00×  | Lower bound — acts at random        |
| Q-Learning       | 42.9 % |   −279,446 |  **−0.028** |  1.00×  | Learned online from TD updates      |
| Double Q-Learning|  ~43 % |        —   |   ~−0.02    |  1.00×  | Twin tables curb max-operator bias  |
| Monte Carlo      | 43.3 % |   −138,700 |  **−0.014** |  1.00×  | Learned from full-episode returns   |
| Basic Strategy   | 43.3 % |    −97,779 |  **−0.010** |  1.00×  | Hand-coded benchmark (near-optimal) |
| **Card Counter** | 43.4 % |  **+37,452** | **+0.0037** |  2.38×  | **Beats the house** (Hi-Lo + spread) |

**Takeaways**

- Starting from zero knowledge, the RL agents converge to within a fraction of a
  percent of expert basic strategy — improving on random play by **>25× in
  expected value**.
- The **card counter is the only agent with a positive edge.** By tracking the
  Hi-Lo count and ramping its bet 1×–12× when the deck is rich, it flips the
  ~1 % house edge into a **+0.37 % player edge** (confirmed at +0.46 % over a
  50,000,000-hand run, ~9σ above zero).
- `agreement_vs_basic` reports how closely a learned policy's greedy actions
  match textbook basic strategy across the full decision grid.

---

## Watch it learn

Run `./blackjack chart mc` to print the policy an agent *learned* as the canonical
strategy grid. Here's Monte Carlo after 1.5 M hands of self-play — it rediscovered
basic strategy on its own (compare with `./blackjack chart basic`):

```
Hard totals          (S = stand, H = hit, D = double)
        2  3  4  5  6  7  8  9 10  A
17-20   S  S  S  S  S  S  S  S  S  S     <- always stand on 17+
16      S  S  S  S  S  H  S  S  S  H
13-15   S  S  S  S  S  H  H  H  H  H     <- stand vs 2-6, hit vs 7-A
12      H  S  S  S  S  H  H  H  H  H
11      D  D  D  D  D  D  D  D  D  D     <- double everything (textbook!)
10      D  D  D  D  D  D  D  D  D  H     <- double vs 2-9, hit vs 10/A (textbook!)
 9      D  D  D  H  D  H  D  H  H  H
```

The high-frequency, high-value decisions (the 11/10 double rows, the 13–16
stand/hit boundary) match the textbook exactly. Marginal cells with tiny EV gaps
stay approximate — honest behaviour for a tabular learner.

Export the same grid to Markdown or CSV with `export-chart`:

```bash
./blackjack export-chart basic --format md --out basic.md
./blackjack export-chart q --in q.policy --format csv --out q.csv
./blackjack export-chart dq --episodes 500000 --format txt
```

---

## Technology stack

Exactly what is built today versus what is scoped as future work — no smoke and
mirrors:

| Area                                | Status        | Where                                            |
| ----------------------------------- | ------------- | ------------------------------------------------ |
| C++ / Object-Oriented design        | ✅ Implemented | `Card`, `Deck`, `Hand`, `Environment`, `Agent` … |
| Data structures & algorithms        | ✅ Implemented | Hash-indexed Q-table, multi-deck shoe, RL control |
| Reinforcement learning / Q-Learning | ✅ Implemented | `QLearningAgent`                                 |
| Double Q-Learning                   | ✅ Implemented | `DoubleQLearningAgent`                           |
| Monte Carlo method                  | ✅ Implemented | `MonteCarloAgent`                                |
| Card counting (Hi-Lo)               | ✅ Implemented | `CountingAgent` + shoe count in `Deck`           |
| Policy agreement metric             | ✅ Implemented | `policyAgreement()` in `Chart`                   |
| Strategy export (md/csv/txt)        | ✅ Implemented | `export-chart` CLI + `exportStrategyChart`       |
| Reproducible runs (`--seed`)        | ✅ Implemented | Seeded `Deck` / `Environment` / agent RNG        |
| Machine learning (learned policy)   | ✅ Implemented | Self-play training + greedy evaluation           |
| Configurable casino rules           | ✅ Implemented | `Rules` (decks, H17/S17, payout, double)         |
| DBMS / SQL                          | ✅ Implemented | SQLite persistence + `sql/schema.sql`            |
| Command-line interface              | ✅ Implemented | Interactive menu + scriptable sub-commands       |
| Graphical user interface            | 🔜 Planned     | See [Roadmap](#roadmap)                          |
| OpenCV (card recognition)           | 🔜 Planned     | See [Roadmap](#roadmap)                          |

---

## How the AI works

### State, actions, reward

The learners see the classic compact Blackjack state and three actions:

```
state   = (player total, dealer up-card, usable ace?, can double?)
actions = { Stand, Hit, Double }
reward  = +1 win,  −1 loss,  0 push,  +1.5 natural,  ±2 after a Double
```

The whole game is one small **Markov Decision Process**, exposed through a
Gym-style `Environment` (`reset()` / `step(action)`). All the rules live in that
one class, so every agent and the evaluator share identical logic.

### Q-Learning

Updates the action-value table after every card with the TD rule
`Q(s,a) ← Q(s,a) + α·[r + γ·maxₐ′ Q(s′,a′) − Q(s,a)]`. Both the exploration rate
ε and the learning rate α are annealed to 0 so the policy settles instead of
hovering.

### Double Q-Learning

Maintains **two** Q-tables (`QA`, `QB`). On each transition one table is chosen at
random to update; the *other* table evaluates the greedy next action chosen from
the updating table. That decoupling removes the positive bias of `maxₐ′ Q(s′,a′)`.
Action selection (and the final greedy policy) uses the **average** of both tables,
which is also what `save()` writes in the standard tabular format. A dual-table
dump (`.dq`) is written alongside when you pass `--out` to `train dq`.

```bash
./blackjack train dq --episodes 500000 --out dq.policy --chart --seed 7
./blackjack eval dq --in dq.policy --hands 200000
./blackjack chart dq --in dq.policy
```

### Monte Carlo control

Plays whole hands with an ε-greedy policy, then nudges each visited
`(state, action)` toward the observed return via an incremental first-visit
average. ε is annealed to 0 (a GLIE schedule) for convergence to optimal play.

### Policy agreement (`agreement_vs_basic`)

`policyAgreement(learned, basic)` walks hard totals 4–21 × dealer up-cards 2–A ×
soft/hard × canDouble, and reports the fraction of states where the learned
agent's greedy action matches basic strategy (legal actions only). Printed by
`train`, `eval`, `chart`, `compare`, and `demo` as `agreement_vs_basic`.

### Card counting

The shoe maintains the **Hi-Lo running count** (2–6 → +1, 7–9 → 0, 10–A → −1).
`CountingAgent` converts it to a *true count* (per deck remaining) and:

- **spreads its bet 1×–12×**, betting big only once the true count clears the
  break-even point (~+1), which is where the player's edge becomes real;
- **deviates from basic strategy** on a subset of the "Illustrious 18" index
  plays the engine's action set can express (e.g. stand on 16 vs 10 at TC ≥ 0).

The bet spread is where a counter's edge actually comes from, so this is the only
agent that ends up ahead of the house.

### Basic strategy (benchmark)

`BasicStrategyAgent` encodes the textbook hard/soft/double table. It learns
nothing — it exists to measure how close the learners get to expert play.

---

## Build

Requires a C++17 compiler. **SQLite is optional** and auto-detected: if present,
run statistics are also written to `blackjack.db`; if not, the build falls back to
CSV-only and still works.

### CMake (recommended)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure   # run the unit tests
./build/blackjack                             # launch
./build/blackjack version                     # → blackjack 1.1.0
```

### Make (no CMake needed)

```bash
make          # builds ./blackjack and ./bjtests
make test     # build + run the tests
make run      # launch the interactive menu
```

---

## Usage

Run with no arguments for an interactive menu. Or drive it from the command line:

```bash
# Train every agent and benchmark them (incl. Double-Q and the card counter):
./blackjack compare --episodes 1000000 --hands 10000000 --seed 2024

# See the strategy an agent learned, as the classic grid:
./blackjack chart mc --episodes 1500000
./blackjack chart dq --episodes 1500000
./blackjack chart basic                       # textbook reference

# Export a strategy grid (markdown / CSV / plain text):
./blackjack export-chart basic --format md --out basic.md
./blackjack export-chart q --in q.policy --format csv --out q.csv

# Train one agent, print its chart, and save the policy:
./blackjack train q  --episodes 500000 --out q.policy  --chart --seed 1
./blackjack train dq --episodes 500000 --out dq.policy --chart --seed 1
./blackjack train mc --episodes 500000 --out mc.policy --chart

# Evaluate a saved/trained policy, or the counter:
./blackjack eval q --in q.policy --hands 200000 --seed 42
./blackjack eval dq --in dq.policy --hands 200000
./blackjack eval count --hands 1000000

# Watch a trained agent play, card by card; or play yourself with hints:
./blackjack watch --hands 5 --seed 99
./blackjack play

# Quick smoke run (Q + Double-Q + MC + basic):
./blackjack demo --seed 7

# Change the rules of any command:
./blackjack compare --decks 2 --h17 --payout 1.2     # 2-deck, H17, 6:5 blackjack
```

### Reproducibility (`--seed`)

Every command that trains or evaluates accepts `--seed N` (default `2024`). The
seed is threaded into:

- `Deck(numDecks, seed)` / `Environment(rules, seed)` — shoe shuffle order
- agent exploration RNGs (`seed + offset` per agent)

Identical seeds + identical episode counts produce identical Q-tables and the
same evaluation shoe sequence across agents (compare uses a shared eval seed so
head-to-heads are fair).

```bash
./blackjack train q --episodes 100000 --seed 123 --out a.policy
./blackjack train q --episodes 100000 --seed 123 --out b.policy
# a.policy and b.policy will match
```

Example interactive hand:

```
Dealer shows: 3♣
Your hand:    Q♣ 10♦ (20)
(hint: Stand)  Action [h/s/d/q]: s
Dealer hand:  3♣ Q♠ K♠ (23, bust)
You WIN  (+1).  Bankroll: 1
```

---

## SQL persistence

Each evaluation run is appended to `stats.csv` and, when built with SQLite, to a
`training_runs` table in `blackjack.db`. The schema and a convenience view live in
[`sql/schema.sql`](sql/schema.sql).

```bash
sqlite3 blackjack.db "SELECT agent, episodes, hands, edge_per_hand
                      FROM training_runs ORDER BY id DESC LIMIT 10;"
sqlite3 blackjack.db "SELECT * FROM best_by_agent;"   # best edge per agent
```

---

## Project layout

```
blackjack-ai/
├── include/blackjack/      Public headers (one class per file)
│   ├── Card, Deck, Hand            – game primitives (Deck tracks the Hi-Lo count)
│   ├── Rules                       – configurable table rules
│   ├── Environment                 – Gym-style MDP wrapper (the rules)
│   ├── Agent, TabularAgent         – policy interface + Q-table base
│   ├── QLearningAgent, DoubleQLearningAgent, MonteCarloAgent,
│   │   BasicStrategyAgent, CountingAgent, RandomAgent  – the agents
│   ├── Chart                       – strategy grid, export, policyAgreement
│   ├── Stats, Persistence          – metrics + CSV/SQLite storage
│   └── Game                        – evaluate / watch / play loops
├── src/                    Implementations + main.cpp (CLI)
├── tests/                  Dependency-free unit tests (CTest)
├── sql/schema.sql          SQLite schema + view
├── docs/DESIGN.md          Architecture & MDP design notes
├── CMakeLists.txt          Primary build
└── Makefile                Lightweight alternative build
```

---

## Testing & CI

A dependency-free test suite (`tests/tests.cpp`) covers card/hand scoring, the
shoe and its Hi-Lo count, deck seed reproducibility, environment + rule
invariants (including H17 vs S17), basic-strategy and counting decisions,
strategy-chart / export generation, policy agreement, Double Q-Learning
finite-Q checks, save/load round trips, and a learning check that Q / Double-Q /
Monte Carlo all beat the random baseline. It runs on every push via **GitHub
Actions** on Linux and macOS.

```bash
ctest --test-dir build --output-on-failure
# or
make test
```

---

## Roadmap

The parts of the original brief not yet built, kept honest as explicit next steps:

- **Graphical user interface** — a table view (e.g. SFML/Qt) rendering hands, the
  shoe, the live count, and agent decisions, replacing the terminal front end.
- **OpenCV card recognition** — read real cards from a webcam/image, map them to
  `Card` objects, and let the trained policy advise on a physical table.
- **Splitting & insurance** — extend the action space and state for a full
  ruleset (and push the counter's edge higher, since insurance is a big part of
  counting EV).
- **Deep Q-Network** — swap the tabular Q-table for a small neural network to
  generalise across states and incorporate the count as a feature.

---

## License

[MIT](LICENSE) © 2026 Sebastian Forbes
