#pragma once

#include "blackjack/Agent.hpp"
#include "blackjack/Stats.hpp"

namespace blackjack {

// Run `hands` hands of an agent's greedy policy and collect statistics.
Stats evaluate(const Agent& agent, long hands, int numDecks = 6,
               unsigned seed = 12345);

// Print a handful of hands of the agent playing, card by card (verbose demo).
void watch(const Agent& agent, int hands, int numDecks = 6, unsigned seed = 777);

// Interactive human game loop (reads commands from stdin). If `advisor` is
// non-null its recommended action is shown as a hint each turn.
void playInteractive(const Agent* advisor = nullptr, int numDecks = 6,
                     unsigned seed = 1);

} // namespace blackjack
