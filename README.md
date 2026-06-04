# Blackjack with AI 🃏

A Blackjack engine in modern **C++17** whose players are driven by
**reinforcement-learning agents** that learn how to play entirely from
self-play — no human strategy is hard-coded into them. Two classic RL methods
are implemented and benchmarked against textbook **basic strategy** and a random
baseline:

- **Q-Learning** — off-policy temporal-difference control
- **Monte Carlo control** — on-policy, first-visit, learning from full-hand returns

Every player and the dealer hold a `Hand` of `Card`s, and each agent decides
what to do based purely on the cards on the table. The project explores how an
agent can *discover* near-optimal Blackjack play through simulated trial and
error.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![License: MIT](https://img.shields.io/badge/license-MIT-green)
![Tests](https://img.shields.io/badge/tests-passing-brightgreen)

---

## Results

Both agents are trained from scratch by self-play, then evaluated on **1,000,000
fresh hands**. `edge/hand` is the average number of betting units won per hand —
closer to `0` is better (the house keeps the rest). Reproduce with
`./blackjack compare --episodes 1000000 --hands 1000000`.

| Agent            | Win %  |   Bankroll | Edge / hand | Notes                                            |
| ---------------- | :----: | ---------: | :---------: | ------------------------------------------------ |
| Random           | 30.2 % | −441,668   |  **−0.442** | Lower bound — hits and stands at random          |
| Q-Learning       | 42.8 % |  −32,517   |  **−0.033** | Learned online from TD updates                   |
| Monte Carlo      | 43.2 % |  −15,574   |  **−0.016** | Learned from full-episode returns                |
| Basic Strategy   | 43.3 % |  −10,285   |  **−0.010** | Hand-coded benchmark (near-optimal)              |

**Takeaway:** starting from zero knowledge, both RL agents converge to within a
fraction of a percent of expert basic strategy and improve on random play by
**>25× in expected value**. Monte Carlo gets closest here; Q-Learning trades a
little accuracy for fully online, incremental updates.

> The slight residual gap to a real casino's ~0.5 % house edge is expected: this
> engine intentionally omits splitting and insurance to keep the decision space
> compact (see [`docs/DESIGN.md`](docs/DESIGN.md)).

---

## Technology stack

The brief for this project lists a broad stack. Here is exactly what is built
today versus what is scoped as future work — no smoke and mirrors:

| Area                              | Status        | Where                                              |
| --------------------------------- | ------------- | -------------------------------------------------- |
| C++ / Object-Oriented design      | ✅ Implemented | `Card`, `Deck`, `Hand`, `Environment`, `Agent` … |
| Data structures & algorithms      | ✅ Implemented | Hash-indexed Q-table, multi-deck shoe, RL control  |
| Reinforcement learning / Q-Learning | ✅ Implemented | `QLearningAgent`                                  |
| Monte Carlo method                | ✅ Implemented | `MonteCarloAgent`                                  |
| Machine learning (learned policy) | ✅ Implemented | Self-play training + greedy evaluation             |
| DBMS / SQL                        | ✅ Implemented | SQLite persistence + `sql/schema.sql`              |
| Command-line interface            | ✅ Implemented | Interactive menu + scriptable sub-commands         |
| Graphical user interface          | 🔜 Planned     | See [Roadmap](#roadmap)                            |
| OpenCV (card recognition)         | 🔜 Planned     | See [Roadmap](#roadmap)                            |

---

## How the AI works

### State, actions, reward

The agents see the classic compact Blackjack state and three actions:

```
state  = (player total, dealer up-card, usable ace?, can double?)
actions = { Stand, Hit, Double }
reward  = +1 win,  −1 loss,  0 push,  +1.5 natural,  ±2 after a Double
```

The whole game is modelled as one small **Markov Decision Process**, exposed
through a Gym-style `Environment` (`reset()` / `step(action)`). All the rules
live in that one class, so every agent and the evaluator share identical logic.

### Q-Learning

Updates the action-value table after every card using the TD rule:

```
Q(s,a) ← Q(s,a) + α · [ r + γ · maxₐ′ Q(s′,a′) − Q(s,a) ]
```

with exploration `ε` annealed linearly to 0 so the greedy policy converges.

### Monte Carlo control

Plays whole hands with an ε-greedy policy, then nudges each visited
`(state, action)` toward the observed return via an incremental sample average
(first-visit). `ε` is annealed to 0 (a GLIE schedule) for convergence to the
optimal policy.

### Basic strategy (benchmark)

`BasicStrategyAgent` encodes the textbook hard/soft/double decision table. It
learns nothing — it exists to measure how close the learners get to expert play.

---

## Build

Requires a C++17 compiler. **SQLite is optional** and auto-detected: if found,
run statistics are also written to `blackjack.db`; if not, the build falls back
to CSV-only and still works.

### CMake (recommended)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure   # run the unit tests
./build/blackjack                             # launch
```

### Make (no CMake needed)

```bash
make          # builds ./blackjack and ./bjtests
make test     # build + run the tests
make run      # launch the interactive menu
```

---

## Usage

Run with no arguments for an interactive menu (play, train, compare, watch).
Or drive it from the command line:

```bash
# Train both learners and benchmark them against basic/random:
./blackjack compare --episodes 1000000 --hands 1000000

# Train one agent and save its learned policy:
./blackjack train q  --episodes 500000 --out q.policy
./blackjack train mc --episodes 500000 --out mc.policy

# Evaluate a saved (or freshly trained) policy:
./blackjack eval q --in q.policy --hands 200000
./blackjack eval basic --hands 200000

# Watch a trained agent play, card by card:
./blackjack watch --hands 5

# Play yourself, with the AI suggesting the basic-strategy move:
./blackjack play
```

Example interactive hand:

```
=========================================
Dealer shows: 3♣
Your hand:    Q♣ 10♦ (20)
(hint: Stand)  Action [h/s/d/q]: s
Dealer hand:  3♣ Q♠ K♠ (23, bust)
You WIN  (+1).  Bankroll: 1
```

---

## SQL persistence

Each evaluation run is appended to `stats.csv` and, when built with SQLite, to a
`training_runs` table in `blackjack.db`. The schema (and a convenience view) is
in [`sql/schema.sql`](sql/schema.sql).

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
│   ├── Card, Deck, Hand            – game primitives
│   ├── Environment                 – Gym-style MDP wrapper (the rules)
│   ├── Agent, TabularAgent         – policy interface + Q-table base
│   ├── QLearningAgent, MonteCarloAgent, BasicStrategyAgent, RandomAgent
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

A dependency-free test suite (`tests/tests.cpp`) covers card/hand scoring,
the shoe, environment invariants, basic-strategy decisions, save/load round
trips, and a learning check that both RL agents beat the random baseline.

```bash
ctest --test-dir build --output-on-failure
# or
make test
```

A ready-to-run **GitHub Actions** workflow (build + test + smoke run on Linux
and macOS) is included at [`ci/ci.yml`](ci/ci.yml). To activate it, move it into
the workflows directory and push with a `workflow`-scoped token:

```bash
mkdir -p .github/workflows && git mv ci/ci.yml .github/workflows/ci.yml
gh auth refresh -h github.com -s workflow   # one-time: grant workflow scope
git commit -am "Enable CI" && git push
```

---

## Roadmap

These are the parts of the original brief not yet built, kept honest as
explicit next steps:

- **Graphical user interface** — a table view (e.g. SFML/Qt) rendering hands,
  the shoe, and live agent decisions, replacing the terminal front end.
- **OpenCV card recognition** — read real cards from a webcam/image, map them to
  `Card` objects, and let the trained policy advise on a physical table.
- **Splitting & insurance** — extend the action space and state for a full
  ruleset, closing the remaining gap to the theoretical house edge.
- **Deep Q-Network** — swap the tabular Q-table for a small neural network to
  generalise across states (and prepare for card-counting features).

---

## License

[MIT](LICENSE) © 2026 Sebastian Forbes
