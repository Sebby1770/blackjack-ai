#pragma once

#include "blackjack/Agent.hpp"
#include "blackjack/BasicStrategyAgent.hpp"

#include <string>
#include <vector>

namespace blackjack {

// One Illustrious-18 / Fab-Four deviation. `decide()` and the CLI table both
// come from `CountingAgent::indexPlays()` so they cannot drift apart.
struct IndexPlay {
    int playerTotal;
    int dealerUp;   // 2..11
    bool soft;
    Action action;
    double threshold;
    bool atLeast;   // true => apply when tc >= threshold; false => when tc < threshold
    const char* name;
};

// A Hi-Lo card counter: it plays basic strategy, deviates on the Illustrious 18
// (and Fab Four, when surrender is legal) when the true count justifies it, and
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

    // Basic strategy plus count-dependent index plays. Always returns a member
    // of `legal` (double/surrender deviations are skipped when not legal).
    Action decide(const State& s, const std::vector<Action>& legal,
                  double trueCount) const;

    // Illustrious 18: take insurance at true count >= +3.
    bool takeInsurance(double trueCount) const { return trueCount >= 3.0; }

    // Wonging: sit out (wager 0) when the true count is at or below -1.
    // The hand is still dealt so the running count keeps moving.
    bool sitOut(double trueCount) const { return trueCount <= -1.0; }

    // I18 (no pair splits — the C++ engine has no split) + Fab Four.
    static std::vector<IndexPlay> indexPlays();
    static std::string exportIndexPlays(bool json);

    // Agent interface: count-agnostic (plays plain basic strategy).
    Action act(const State& s, const std::vector<Action>& legal) const override {
        return decide(s, legal, 0.0);
    }

    std::string name() const override { return "Card-Counter"; }

private:
    BasicStrategyAgent basic_;
};

} // namespace blackjack
