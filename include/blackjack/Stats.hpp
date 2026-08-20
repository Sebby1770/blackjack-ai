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
    double wagered    = 0.0;   // total units staked (only tracked when bets vary)
    double rewardSumSq = 0.0;  // for the standard error of the mean edge

    void record(double reward, bool playerBlackjack = false);

    double winRate() const     { return hands ? static_cast<double>(wins) / hands : 0.0; }
    double edgePerHand() const { return hands ? bankroll / hands : 0.0; }
    // Standard error of edge/hand (sample stdev / sqrt(n)).
    double edgeStderr() const;
    // Average bet in units. Flat-betting agents don't track wagered, so report
    // the implied 1.0 unit; the counter accumulates its varying stake.
    double avgBet() const {
        if (!hands) return 0.0;
        return wagered > 0.0 ? wagered / hands : 1.0;
    }

    std::string summary() const;
};

} // namespace blackjack
