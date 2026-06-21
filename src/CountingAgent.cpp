#include "blackjack/CountingAgent.hpp"

#include <algorithm>

namespace blackjack {

double CountingAgent::betUnits(double tc) const {
    // The player only gains the advantage once the true count clears ~+1
    // (each +1 TC is worth roughly +0.5% in a 6-deck shoe), so the spread
    // keeps the table minimum until then and ramps hard where the edge is real.
    if (tc < 1.0) return 1.0;
    if (tc < 2.0) return 2.0;
    if (tc < 3.0) return 5.0;
    if (tc < 4.0) return 9.0;
    return 12.0;
}

Action CountingAgent::decide(const State& s, const std::vector<Action>& legal,
                             double tc) const {
    const bool canDouble =
        std::find(legal.begin(), legal.end(), Action::Double) != legal.end();

    // Index-play deviations on hard totals (subset of the "Illustrious 18" that
    // doesn't require insurance or splitting). Each overrides basic strategy
    // only past its count threshold; Stand and Hit are always legal, and the
    // Double deviations are gated on doubling being available.
    if (!s.usableAce) {
        const int t = s.playerTotal;
        const int d = s.dealerUpValue;   // 2..11 (11 = ace)

        if (t == 16 && d == 10 && tc >= 0.0) return Action::Stand;
        if (t == 16 && d == 9  && tc >= 4.0) return Action::Stand;
        if (t == 15 && d == 10 && tc >= 4.0) return Action::Stand;
        if (t == 13 && d == 2  && tc <= -1.0) return Action::Hit;
        if (t == 12 && d == 3  && tc >= 2.0) return Action::Stand;
        if (t == 12 && d == 2  && tc >= 3.0) return Action::Stand;
        if (t == 12 && d == 4  && tc <  0.0) return Action::Hit;

        if (canDouble) {
            if (t == 10 && d == 10 && tc >= 4.0) return Action::Double;
            if (t == 10 && d == 11 && tc >= 4.0) return Action::Double;
            if (t == 11 && d == 11 && tc >= 1.0) return Action::Double;
            if (t == 9  && d == 2  && tc >= 1.0) return Action::Double;
            if (t == 9  && d == 7  && tc >= 3.0) return Action::Double;
        }
    }

    return basic_.act(s, legal);
}

} // namespace blackjack
