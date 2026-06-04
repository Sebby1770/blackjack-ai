#include "blackjack/BasicStrategyAgent.hpp"

#include <algorithm>

namespace blackjack {

// Multi-deck, dealer-stands-on-17 basic strategy, restricted to the actions
// this engine supports (no splits/surrender). `dealerUpValue` is 2..11 with
// 11 meaning the dealer shows an ace.
Action BasicStrategyAgent::act(const State& s, const std::vector<Action>& legal) const {
    const bool canDouble =
        std::find(legal.begin(), legal.end(), Action::Double) != legal.end();
    const int  t    = s.playerTotal;
    const int  d    = s.dealerUpValue;
    const bool soft = s.usableAce;

    // Double when the table says so, otherwise fall back to `alt`.
    auto dbl = [&](Action alt) { return canDouble ? Action::Double : alt; };

    if (soft) {
        // Soft totals (one ace still counted as 11).
        if (t >= 19) return Action::Stand;                       // soft 19, 20, 21
        if (t == 18) {                                           // A,7
            if (d >= 3 && d <= 6)               return dbl(Action::Stand);
            if (d == 2 || d == 7 || d == 8)     return Action::Stand;
            return Action::Hit;                                  // vs 9, 10, A
        }
        if (t == 17) return (d >= 3 && d <= 6) ? dbl(Action::Hit) : Action::Hit;
        if (t == 16 || t == 15) return (d >= 4 && d <= 6) ? dbl(Action::Hit) : Action::Hit;
        if (t == 14 || t == 13) return (d >= 5 && d <= 6) ? dbl(Action::Hit) : Action::Hit;
        return Action::Hit;
    }

    // Hard totals.
    if (t >= 17)            return Action::Stand;
    if (t >= 13 && t <= 16) return (d >= 2 && d <= 6) ? Action::Stand : Action::Hit;
    if (t == 12)            return (d >= 4 && d <= 6) ? Action::Stand : Action::Hit;
    if (t == 11)            return dbl(Action::Hit);
    if (t == 10)            return (d >= 2 && d <= 9) ? dbl(Action::Hit) : Action::Hit;
    if (t == 9)             return (d >= 3 && d <= 6) ? dbl(Action::Hit) : Action::Hit;
    return Action::Hit;                                          // hard 8 or less
}

} // namespace blackjack
