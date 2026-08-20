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
bool twoCardDecision(const Hand& h, bool doubled) {
    return h.size() == 2 && !doubled;
}
bool canDoubleNow(const Rules& r, const Hand& h, bool doubled) {
    return r.allowDouble && twoCardDecision(h, doubled);
}
bool canSurrenderNow(const Rules& r, const Hand& h, bool doubled) {
    return r.allowSurrender && twoCardDecision(h, doubled);
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
    if (canSurrenderNow(rules_, player_, doubled_)) legal.push_back(Action::Surrender);
    return legal;
}

Environment::Step Environment::reset() {
    if (deck_.remaining() < rules_.penetration * deck_.size()) deck_.shuffle();
    player_.clear();
    dealer_.clear();
    bet_         = 1.0;
    doubled_     = false;
    done_        = false;
    holeHidden_  = false;

    // Standard deal order: player, dealer up, player, dealer hole.
    // The hole card is not Hi-Lo counted until it is turned up.
    player_.add(deck_.deal(true));
    dealer_.add(deck_.deal(true));
    player_.add(deck_.deal(true));
    dealer_.add(deck_.deal(false));
    holeHidden_ = true;

    const bool playerBJ = player_.isBlackjack();
    const bool dealerBJ = dealer_.isBlackjack();
    if (playerBJ || dealerBJ) {
        revealHole();
        done_ = true;
        double r = 0.0;
        if (playerBJ && dealerBJ) r = 0.0;                     // both naturals -> push
        else if (playerBJ)        r = rules_.blackjackPayout;  // natural pays 3:2 by default
        else                      r = -1.0;                    // dealer natural
        return {observe(), r, true, {}};
    }
    return {observe(), 0.0, false, legalActions()};
}

void Environment::revealHole() {
    if (!holeHidden_) return;
    if (dealer_.cards().size() >= 2) deck_.count(dealer_.cards()[1]);
    holeHidden_ = false;
}

Environment::Step Environment::step(Action a) {
    // Doubling is only legal on the opening two-card decision; otherwise treat
    // it as a hit so a stray request can never corrupt the hand.
    if (a == Action::Double && !canDoubleNow(rules_, player_, doubled_)) {
        a = Action::Hit;
    }

    if (a == Action::Surrender && canSurrenderNow(rules_, player_, doubled_)) {
        revealHole();
        done_ = true;
        return {observe(), -0.5 * bet_, true, {}};
    }

    if (a == Action::Hit) {
        player_.add(deck_.deal(true));
        if (player_.isBust()) {
            revealHole();
            done_ = true;
            return {observe(), -bet_, true, {}};
        }
        return {observe(), 0.0, false, legalActions()};
    }

    if (a == Action::Double) {
        doubled_ = true;
        bet_     = 2.0;
        player_.add(deck_.deal(true));             // exactly one card on a double
        done_    = true;
        if (player_.isBust()) {
            revealHole();
            return {observe(), -bet_, true, {}};
        }
        return {observe(), playOutDealer(), true, {}};
    }

    // Stand (or an illegal surrender treated as stand).
    done_ = true;
    return {observe(), playOutDealer(), true, {}};
}

double Environment::playOutDealer() {
    revealHole();
    // Dealer draws to 17, optionally hitting soft 17 (H17).
    while (dealer_.value() < 17 ||
           (rules_.dealerHitsSoft17 && dealer_.value() == 17 && dealer_.isSoft())) {
        dealer_.add(deck_.deal(true));
    }
    const int p = player_.value();
    const int d = dealer_.value();
    if (d > 21) return  bet_;                       // dealer bust
    if (p > d)  return  bet_;
    if (p < d)  return -bet_;
    return 0.0;                                     // push
}

} // namespace blackjack
