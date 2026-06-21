#pragma once

#include <vector>

#include "blackjack/Action.hpp"
#include "blackjack/Deck.hpp"
#include "blackjack/Hand.hpp"
#include "blackjack/Rules.hpp"

namespace blackjack {

// A Gym-style single-player blackjack environment. The agent plays one hand
// against an automated dealer. Rewards are in betting units: +1 win, -1 loss,
// 0 push, +blackjackPayout for a natural, and doubled (+/-2) after a Double.
// Every learning agent and the evaluator drive the table through this one
// class, so the rules live in exactly one place.
class Environment {
public:
    struct Step {
        State  state;
        double reward = 0.0;
        bool   done = true;
        std::vector<Action> legal;
    };

    explicit Environment(const Rules& rules = Rules{}, unsigned seed = 2024);

    Step reset();                     // deal a new hand, return the opening step
    Step step(Action a);              // apply an action, return the next step

    // Accessors for rendering / interactive play.
    const Hand& player() const { return player_; }
    const Hand& dealer() const { return dealer_; }
    Card dealerUpCard() const { return dealer_.cards().front(); }
    bool done() const { return done_; }
    const Rules& rules() const { return rules_; }

    // Card-counting hooks (Hi-Lo). The count carries across hands within a shoe
    // and resets on each shuffle.
    int    runningCount() const { return deck_.runningCount(); }
    double decksRemaining() const { return deck_.decksRemaining(); }
    double trueCount() const;

private:
    State observe() const;
    std::vector<Action> legalActions() const;
    double playOutDealer();           // dealer draws per the rules, returns payoff

    Rules  rules_;
    Deck   deck_;
    Hand   player_;
    Hand   dealer_;
    double bet_ = 1.0;
    bool   doubled_ = false;
    bool   done_ = true;
};

} // namespace blackjack
