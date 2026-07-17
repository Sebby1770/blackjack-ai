# Design notes

This document explains how the project is put together and why the key
decisions were made. It complements the inline comments in the headers.

## Goals

1. A correct, well-factored Blackjack engine in idiomatic C++17.
2. Five reinforcement-learning agents that learn good play from self-play alone
   (Q-Learning, Double Q-Learning, SARSA, Expected SARSA, Monte Carlo).
3. A hand-coded basic-strategy benchmark to measure them against.
4. Reproducible, scriptable evaluation with CSV/SQL persistence (`--seed`),
   machine-readable `--json` summaries, and HTML strategy-chart export.

## Architecture

The codebase is a small layered design — primitives at the bottom, the rules in
the middle, policies on top, and a thin CLI on the side.

```
            ┌────────────────────────── main.cpp (CLI) ──────────────────────────┐
            │  interactive menu · train · eval · compare · export-chart · demo    │
            └─────────────────────────────────┬───────────────────────────────────┘
                                              │
          ┌───────────────────────┼───────────────────┬──────────────┐
          │                        │                   │              │
   evaluate()/watch()       Agent (interface)     strategyChart   StatsStore
   evaluateCounting()   ┌───────┬──┴──────┬─────────┐ + agreement  (CSV + SQLite)
   playInteractive()    │       │         │         │ + export
          │      TabularAgent  Basic-  Counting  RandomAgent
          │   ┌─────┼─────┬──────┬────────┐   Strategy  Agent
          │  QLearn DoubleQ SARSA Expected  MonteCarlo
          │  Agent  Agent   Agent SARSA     Agent
          │
   ┌──────┴──────────── Environment ─────────────┐   ← all rules live here
   │  reset() / step(action) → {state, reward}   │   ← Rules configure it
   │  trueCount()  (Hi-Lo, for the counter)      │
   └──────────────┬──────────────┬───────────────┘
                  │              │
                Deck           Hand
            (+ Hi-Lo count)      │
                  └──── Card ────┘
```

**Why a single `Environment`?** Blackjack rules are easy to get subtly wrong
(soft aces, when doubling is legal, dealer draw rules, naturals). Putting them
in exactly one Gym-style class means the human game, the evaluator, and both
learners all exercise the same code — there is no second implementation to drift
out of sync.

**Why `TabularAgent` as a base?** Q-Learning, Double Q-Learning, SARSA, Expected
SARSA, and Monte Carlo differ only in *how* they update the table(s). Sharing the
table, the greedy / ε-greedy policies, `expectedEpsilonGreedyQ`, and save/load
keeps each agent down to just its learning rule. Double Q keeps two internal
tables and syncs their average into the base `q_` so `act()` / `save()` stay
compatible with the single-table format.

### Double Q-Learning

Standard Q-Learning's bootstrap target `max_a' Q(s', a')` is a source of
positive bias: the same noisy estimates both choose and evaluate the next action.
Double Q-Learning (Hasselt, 2010) stores two tables `QA` and `QB`. On each
transition, one table is picked at random to update; the *other* scores the
greedy action chosen from the updating table:

```
with probability 1/2:
    a* = argmax_a' QA(s', a')
    QA(s,a) ← QA(s,a) + α [ r + γ QB(s', a*) − QA(s,a) ]
else: roles of QA / QB swapped
```

ε-greedy selection and the final greedy policy use `(QA + QB) / 2`. That average
is what the standard tabular save format stores; `saveDual` / `loadDual` persist
both tables for continued dual training.

### SARSA and Expected SARSA

**SARSA** is on-policy TD control: the bootstrap uses the *next action actually
chosen* under the current ε-greedy policy,
`Q(s,a) ← Q(s,a) + α [ r + γ Q(s′,a′) − Q(s,a) ]`. As ε anneals to 0 the
behaviour policy becomes greedy, so the learned Q converges toward the optimal
action values without the off-policy max operator.

**Expected SARSA** replaces the sampled `Q(s′,a′)` with the expectation under the
same ε-greedy policy:
`E[Q] = (1−ε)·max_a′ Q(s′,a′) + ε·mean_a′ Q(s′,a′)` (implemented as
`TabularAgent::expectedEpsilonGreedyQ`). Lower variance than SARSA; still
on-policy. When ε = 0 this collapses to the Q-Learning target.

### Policy agreement

`policyAgreement(learned, basic)` enumerates the decision grid
(hard totals 4–21 × dealer 2–A × soft/hard × canDouble) and returns the fraction
of states where both agents pick the same legal greedy action. It is a cheap,
interpretable proxy for "how close did the learner get to textbook play" that
does not require millions of evaluation hands.

### JSON CLI and training progress

`train` / `eval` / `compare` / `demo` accept `--json` and emit a single JSON
object (agents array for multi-agent commands) suitable for scripting. Progress
during self-play is optional (`--progress` ≈ every 10 %, or `--progress-every N`)
and writes to **stderr** so stdout stays clean for `--json` pipelines.

### Parallel evaluation

`compare` and `demo` score agents with independent `std::thread`s. Each call to
`evaluate` owns its own `Environment`; agents are not mutated during scoring, so
there are no data races. Training remains sequential (shared RNG/config
simplicity).

## The MDP

| Element | Choice |
| ------- | ------ |
| State   | `(playerTotal, dealerUpValue, usableAce, canDouble)` |
| Actions | `Stand`, `Hit`, `Double` |
| Reward  | `+1` win, `−1` loss, `0` push, `+1.5` natural, doubled after `Double` |
| Discount| `γ = 1` (episodic, undiscounted — hands are short) |

`canDouble` is part of the state (not just an action mask) so the agents can
learn a *different* value for a hand on the opening decision versus the same
total reached after hitting — doubling is only legal on the first two cards.

The state space is tiny (≈ `21 × 10 × 2 × 2 × 3` entries), so a hash-indexed
table converges fast and needs no function approximation.

### Reward & return

Only the terminal transition carries a non-zero reward. With `γ = 1` this means
every `(state, action)` in a hand shares the same return `G` — the final
payoff. That keeps the Monte Carlo update a plain running average and makes the
Q-Learning bootstrap collapse to the terminal reward at the leaf.

## Card counting

`CountingAgent` is the one agent that ends up ahead of the house. The shoe
(`Deck`) keeps a Hi-Lo **running count** of every card dealt since the last
shuffle; `Environment` exposes it and the derived **true count** (running ÷ decks
remaining). The agent then does two things:

- **Bet spread.** It bets the table minimum until the true count clears the
  break-even point (~+1, since each +1 TC is worth roughly +0.5 %), then ramps
  1×→12×. This is where essentially all of a counter's edge comes from.
- **Index deviations.** A handful of the "Illustrious 18" plays expressible in
  this action set (no insurance/splits), e.g. stand on 16 vs 10 once the count
  is neutral-or-better.

Because counting needs information beyond a single hand (the shoe's count), it is
driven by `evaluateCounting`, which reads the true count *before* each deal to
size the bet, rather than the count-blind `evaluate`.

One simplification: the running count includes the dealer's hole card, which a
real player can't see until it's flipped. It slightly idealises the count but
keeps the environment to a single source of truth.

## Strategy charts

`strategyChart(const Agent&)` renders any policy as the canonical grid by querying
`agent.act()` for every (player total, dealer up-card) cell with a fresh two-card
hand. Because it only depends on the `Agent` interface, the same function visualises
a hand-coded table or a learned Q-table — which is what lets you watch Monte Carlo
rediscover basic strategy.

## Deliberate simplifications

- **No splitting / insurance (v1.2 still).** These multiply the state and action
  space (multiple simultaneous hands, side bets) for little additional learning
  interest relative to shipping solid TD agents + export/CLI. The result is a
  small, honest extra house edge versus a full ruleset — documented, not hidden.
  Splitting / insurance remain on the roadmap; v1.2 prioritised SARSA, Expected
  SARSA, HTML charts, and JSON CLI instead of a half-broken ruleset expansion.
- **Configurable rules, sensible defaults.** Deck count, S17/H17, blackjack
  payout, doubling, and shoe penetration live in `Rules`; the defaults model a
  common 6-deck, S17, 3:2 game with 80 % penetration.
- **Even-money payouts** except naturals (3:2). Keeps the RL reward signal
  clean while still rewarding blackjacks correctly.
- **Tabular, not neural.** The state space is small enough that a table is both
  optimal and interpretable. A DQN is on the roadmap as a generalisation study.

## Reproducibility

Every randomised component (shoe, exploration) is seeded. Training and
evaluation use fixed seeds by default, so `compare` produces the same numbers
run to run, and evaluation deals identical hands to each agent for a fair
head-to-head. The CLI exposes this as `--seed N` on train / eval / compare /
chart / export-chart / watch / demo / play; the value is forwarded into
`Environment(rules, seed)` (and thus `Deck`) and each agent's exploration RNG
(`seed +` a small per-agent offset).

## Extending it

- **New agent:** subclass `Agent` (or `TabularAgent`) and implement `act()` /
  `train()`. It immediately works with `evaluate`, `watch`, `strategyChart`,
  `policyAgreement`, `exportStrategyChart`, and the CLI.
- **Rule change:** add a field to `Rules`; honour it in `Environment` only.
- **New metric:** add a field to `Stats` and a column in `Persistence` /
  `sql/schema.sql`.
