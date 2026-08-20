#pragma once

#include "blackjack/Agent.hpp"
#include "blackjack/BasicStrategyAgent.hpp"

namespace blackjack {

// A Hi-Lo card counter: it plays basic strategy, deviates on a handful of
// high-value "index" plays when the true count justifies it, and -- crucially --
// ramps its bet up as the count rises. The bet spread is where a counter's
// edge actually comes from, so this is the agent that can beat the house.
//
// Because counting needs information beyond a single hand's cards (the running
// count of the shoe), it is driven by `evaluateCounting` rather than the
// count-blind `evaluate`. Its plain `act` falls back to basic strategy.
class CountingAgent : public Agent {
public:
    // Bet size in units as a function of the Hi-Lo true count (a 1x..8x spread).
    double betUnits(double trueCount) const;

    // Basic strategy plus the count-dependent deviations the engine's action
    // set (no insurance/splits) can express.
    Action decide(const State& s, const std::vector<Action>& legal,
                  double trueCount) const;

    // Illustrious 18: take insurance at true count >= +3.
    bool takeInsurance(double trueCount) const { return trueCount >= 3.0; }

    // Agent interface: count-agnostic (plays plain basic strategy).
    Action act(const State& s, const std::vector<Action>& legal) const override {
        return decide(s, legal, 0.0);
    }

    std::string name() const override { return "Card-Counter"; }

private:
    BasicStrategyAgent basic_;
};

} // namespace blackjack
