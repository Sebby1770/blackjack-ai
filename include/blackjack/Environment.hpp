#pragma once

#include <vector>

#include "blackjack/Action.hpp"
#include "blackjack/Deck.hpp"
#include "blackjack/Hand.hpp"

namespace blackjack {

// A Gym-style single-player blackjack environment. The agent plays one hand
// against an automated dealer (stands on all 17). Rewards are in betting units:
// +1 win, -1 loss, 0 push, +1.5 for a natural, and doubled (+/-2) after a
// Double. Every learning agent and the evaluator drive the table through this
// one class, so the rules live in exactly one place.
class Environment {
public:
    struct Step {
        State  state;                 // observation after the transition
        double reward = 0.0;          // reward received on this transition
        bool   done = true;           // is the hand finished?
        std::vector<Action> legal;    // legal actions from `state` (empty if done)
    };

    explicit Environment(int numDecks = 6, unsigned seed = 2024);

    Step reset();                     // deal a new hand, return the opening step
    Step step(Action a);              // apply an action, return the next step

    // Accessors for rendering / interactive play.
    const Hand& player() const { return player_; }
    const Hand& dealer() const { return dealer_; }
    Card dealerUpCard() const { return dealer_.cards().front(); }
    bool done() const { return done_; }

private:
    State observe() const;
    std::vector<Action> legalActions() const;
    double playOutDealer();           // dealer draws to 17+, returns payoff

    Deck   deck_;
    Hand   player_;
    Hand   dealer_;
    double bet_ = 1.0;
    bool   doubled_ = false;
    bool   done_ = true;
};

} // namespace blackjack
