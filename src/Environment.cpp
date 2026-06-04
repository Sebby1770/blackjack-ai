#include "blackjack/Environment.hpp"

namespace blackjack {

Environment::Environment(int numDecks, unsigned seed) : deck_(numDecks, seed) {}

State Environment::observe() const {
    State s;
    s.playerTotal   = player_.value();
    s.dealerUpValue = dealerUpCard().value();      // ace shows as 11
    s.usableAce     = player_.isSoft();
    s.canDouble     = (player_.size() == 2 && !doubled_);
    return s;
}

std::vector<Action> Environment::legalActions() const {
    std::vector<Action> legal{Action::Stand, Action::Hit};
    if (player_.size() == 2 && !doubled_) legal.push_back(Action::Double);
    return legal;
}

Environment::Step Environment::reset() {
    if (deck_.needsReshuffle()) deck_.shuffle();
    player_.clear();
    dealer_.clear();
    bet_     = 1.0;
    doubled_ = false;
    done_    = false;

    // Standard deal order: player, dealer up, player, dealer hole.
    player_.add(deck_.deal());
    dealer_.add(deck_.deal());
    player_.add(deck_.deal());
    dealer_.add(deck_.deal());

    const bool playerBJ = player_.isBlackjack();
    const bool dealerBJ = dealer_.isBlackjack();
    if (playerBJ || dealerBJ) {
        done_ = true;
        double r = 0.0;
        if (playerBJ && dealerBJ) r =  0.0;        // both naturals -> push
        else if (playerBJ)        r =  1.5;        // natural pays 3:2
        else                      r = -1.0;        // dealer natural
        return {observe(), r, true, {}};
    }
    return {observe(), 0.0, false, legalActions()};
}

Environment::Step Environment::step(Action a) {
    // Doubling is only legal on the opening two-card decision; otherwise treat
    // it as a hit so a stray request can never corrupt the hand.
    if (a == Action::Double && !(player_.size() == 2 && !doubled_)) {
        a = Action::Hit;
    }

    if (a == Action::Hit) {
        player_.add(deck_.deal());
        if (player_.isBust()) {
            done_ = true;
            return {observe(), -bet_, true, {}};
        }
        return {observe(), 0.0, false, legalActions()};
    }

    if (a == Action::Double) {
        doubled_ = true;
        bet_     = 2.0;
        player_.add(deck_.deal());                 // exactly one card on a double
        done_    = true;
        if (player_.isBust()) return {observe(), -bet_, true, {}};
        return {observe(), playOutDealer(), true, {}};
    }

    // Stand.
    done_ = true;
    return {observe(), playOutDealer(), true, {}};
}

double Environment::playOutDealer() {
    while (dealer_.value() < 17) {                 // dealer stands on all 17 (S17)
        dealer_.add(deck_.deal());
    }
    const int p = player_.value();
    const int d = dealer_.value();
    if (d > 21) return  bet_;                       // dealer bust
    if (p > d)  return  bet_;
    if (p < d)  return -bet_;
    return 0.0;                                     // push
}

} // namespace blackjack
