#include "blackjack/Environment.hpp"

#include <algorithm>

namespace blackjack {

Environment::Environment(const Rules& rules, unsigned seed)
    : rules_(rules), deck_(rules.numDecks, seed) {}

double Environment::trueCount() const {
    // True count = running count per deck remaining (standard Hi-Lo). Guard the
    // denominator so a nearly-empty shoe can't blow it up.
    return runningCount() / std::max(1.0, decksRemaining());
}

namespace {
bool canDoubleNow(const Rules& r, const Hand& h, bool doubled) {
    return r.allowDouble && h.size() == 2 && !doubled;
}
} // namespace

State Environment::observe() const {
    State s;
    s.playerTotal   = player_.value();
    s.dealerUpValue = dealerUpCard().value();      // ace shows as 11
    s.usableAce     = player_.isSoft();
    s.canDouble     = canDoubleNow(rules_, player_, doubled_);
    return s;
}

std::vector<Action> Environment::legalActions() const {
    std::vector<Action> legal{Action::Stand, Action::Hit};
    if (canDoubleNow(rules_, player_, doubled_)) legal.push_back(Action::Double);
    return legal;
}

Environment::Step Environment::reset() {
    if (deck_.remaining() < rules_.penetration * deck_.size()) deck_.shuffle();
    player_.clear();
    dealer_.clear();
    bet_     = 1.0;
    doubled_ = false;
    done_    = false;

    // Standard deal order: player, dealer up, player, dealer hole.
    player_.add(deck_.deal());
    dealer_.add(deck_.deal());
    player_.add(deck_.deal());
    dealer_.add(deck_.dealHidden());   // face down: not visible to a counter yet
    holeRevealed_ = false;

    const bool playerBJ = player_.isBlackjack();
    const bool dealerBJ = dealer_.isBlackjack();
    if (playerBJ || dealerBJ) {
        done_ = true;
        revealHoleCard();              // naturals are always turned over

        double r = 0.0;
        if (playerBJ && dealerBJ) r = 0.0;                     // both naturals -> push
        else if (playerBJ)        r = rules_.blackjackPayout;  // natural pays 3:2 by default
        else                      r = -1.0;                    // dealer natural
        return {observe(), r, true, {}};
    }
    return {observe(), 0.0, false, legalActions()};
}

Environment::Step Environment::step(Action a) {
    // Doubling is only legal on the opening two-card decision; otherwise treat
    // it as a hit so a stray request can never corrupt the hand.
    if (a == Action::Double && !canDoubleNow(rules_, player_, doubled_)) {
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

void Environment::revealHoleCard() {
    if (!holeRevealed_) {
        holeRevealed_ = true;
        deck_.reveal(dealer_.cards().back());
    }
}

double Environment::playOutDealer() {
    revealHoleCard();                  // the dealer turns it over before drawing
    // Dealer draws to 17, optionally hitting soft 17 (H17).
    while (dealer_.value() < 17 ||
           (rules_.dealerHitsSoft17 && dealer_.value() == 17 && dealer_.isSoft())) {
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
