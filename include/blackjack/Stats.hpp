#pragma once

#include <string>

namespace blackjack {

// Running tally of an agent's results over many hands.
struct Stats {
    long   hands      = 0;
    long   wins       = 0;
    long   losses     = 0;
    long   pushes     = 0;
    long   blackjacks = 0;
    double bankroll   = 0.0;   // net units won/lost

    void record(double reward, bool playerBlackjack = false);

    double winRate() const     { return hands ? static_cast<double>(wins) / hands : 0.0; }
    double edgePerHand() const { return hands ? bankroll / hands : 0.0; }

    std::string summary() const;
};

} // namespace blackjack
