#pragma once

#include "blackjack/Agent.hpp"
#include "blackjack/Rules.hpp"
#include "blackjack/Stats.hpp"

namespace blackjack {

class CountingAgent;  // defined in CountingAgent.hpp; used by reference below

// Run `hands` hands of an agent's greedy policy and collect statistics.
Stats evaluate(const Agent& agent, long hands, const Rules& rules = Rules{},
               unsigned seed = 12345);

// Like evaluate, but feeds the live Hi-Lo true count to the counter so it can
// vary its bet and make index-play deviations. Bankroll reflects the varying
// stake, which is how the card-counting edge shows up.
Stats evaluateCounting(const CountingAgent& agent, long hands,
                       const Rules& rules = Rules{}, unsigned seed = 24680);

// Print a handful of hands of the agent playing, card by card (verbose demo).
void watch(const Agent& agent, int hands, const Rules& rules = Rules{},
           unsigned seed = 777);

// Interactive human game loop (reads commands from stdin). If `advisor` is
// non-null its recommended action is shown as a hint each turn.
void playInteractive(const Agent* advisor = nullptr, const Rules& rules = Rules{},
                     unsigned seed = 1);

} // namespace blackjack
