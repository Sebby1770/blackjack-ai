# Design notes

This document explains how the project is put together and why the key
decisions were made. It complements the inline comments in the headers.

## Goals

1. A correct, well-factored Blackjack engine in idiomatic C++17.
2. Two reinforcement-learning agents that learn good play from self-play alone.
3. A hand-coded basic-strategy benchmark to measure them against.
4. Reproducible, scriptable evaluation with CSV/SQL persistence.

## Architecture

The codebase is a small layered design — primitives at the bottom, the rules in
the middle, policies on top, and a thin CLI on the side.

```
            ┌────────────────────────── main.cpp (CLI) ──────────────────────────┐
            │  interactive menu · train · eval · compare · watch · play · demo    │
            └─────────────────────────────────┬───────────────────────────────────┘
                                              │
                    ┌─────────────────────────┼──────────────────────────┐
                    │                          │                          │
              evaluate()/watch()        Agent (interface)            StatsStore
              playInteractive()     ┌────────┼────────────┐      (CSV + SQLite)
                    │               │        │            │
                    │        TabularAgent  Basic-     RandomAgent
                    │        ┌────┴────┐   Strategy
                    │   QLearning   MonteCarlo
                    │   Agent       Agent
                    │
              ┌─────┴───────────── Environment ─────────────┐   ← all rules live here
              │     reset() / step(action) → {state, reward}│
              └───────────────┬──────────────┬──────────────┘
                              │              │
                            Deck           Hand
                              │              │
                              └──── Card ────┘
```

**Why a single `Environment`?** Blackjack rules are easy to get subtly wrong
(soft aces, when doubling is legal, dealer draw rules, naturals). Putting them
in exactly one Gym-style class means the human game, the evaluator, and both
learners all exercise the same code — there is no second implementation to drift
out of sync.

**Why `TabularAgent` as a base?** Q-Learning and Monte Carlo differ only in
*how* they update the table. Sharing the table, the greedy / ε-greedy policies,
and save/load keeps each agent down to just its learning rule.

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

## Deliberate simplifications

- **No splitting / insurance.** These multiply the state and action space
  (multiple simultaneous hands) for little additional learning interest. The
  result is a small, honest extra house edge versus a full ruleset — documented,
  not hidden. Splitting is the first item on the roadmap.
- **Dealer stands on all 17 (S17).** The most common rule; one line in
  `Environment::playOutDealer()` to switch to H17.
- **Even-money payouts** except naturals (3:2). Keeps the RL reward signal
  clean while still rewarding blackjacks correctly.
- **Tabular, not neural.** The state space is small enough that a table is both
  optimal and interpretable. A DQN is on the roadmap as a generalisation study.

## Reproducibility

Every randomised component (shoe, exploration) is seeded. Training and
evaluation use fixed seeds by default, so `compare` produces the same numbers
run to run, and evaluation deals identical hands to each agent for a fair
head-to-head.

## Extending it

- **New agent:** subclass `Agent` (or `TabularAgent`) and implement `act()` /
  `train()`. It immediately works with `evaluate`, `watch`, and the CLI.
- **Rule change:** edit `Environment` only.
- **New metric:** add a field to `Stats` and a column in `Persistence` /
  `sql/schema.sql`.
