#pragma once

namespace blackjack {

// Configurable table rules. Defaults model a common 6-deck game: dealer stands
// on all 17, blackjack pays 3:2, doubling allowed on any two cards. Tweaking
// these lets you measure how the house edge (and the agents) respond.
struct Rules {
    int    numDecks         = 6;
    bool   dealerHitsSoft17 = false;   // false = S17, true = H17
    double blackjackPayout  = 1.5;     // 3:2 natural
    bool   allowDouble      = true;
    bool   allowSurrender   = true;    // late surrender: −0.5 units on the first decision
    bool   allowInsurance   = true;    // offered when the dealer shows an ace
    // Reshuffle once fewer than this fraction of the shoe remains (the cut
    // card). 0.20 == 80% penetration, typical of a deep-dealt shoe and the
    // kind of game a card counter looks for. Smaller = better for counting.
    double penetration      = 0.20;
};

} // namespace blackjack
