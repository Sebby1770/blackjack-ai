#pragma once

#include "blackjack/Rules.hpp"

namespace blackjack {

// Infinite-deck action values for a compact blackjack state. Tens are 4/13,
// each other rank 1/13. Dealer plays S17 or H17 per Rules. These are
// teaching numbers, not a shoe-composition engine.
struct ActionEV {
    double stand     = 0.0;
    double hit       = 0.0;   // draw one card, then stand
    double doubleDown = 0.0;  // one card, stand, 2x payoff
    double surrender = -0.5;
};

ActionEV infiniteDeckEV(int playerTotal, int dealerUp, bool soft,
                        const Rules& rules = Rules{});

// Play `hands` with the counting agent, resizing the bet to remaining
// bankroll. Ruin = bankroll < 1 unit.
struct BankrollPath {
    double start     = 0.0;
    double end       = 0.0;
    double minBank   = 0.0;
    double maxBank   = 0.0;
    long   hands     = 0;
    long   ruinedAt  = 0;   // 0 if never ruined
};

class CountingAgent;

BankrollPath simulateBankroll(const CountingAgent& agent, double bankroll,
                              long hands, const Rules& rules = Rules{},
                              unsigned seed = 2024);

} // namespace blackjack
