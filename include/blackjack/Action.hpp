#pragma once

#include <cstddef>

namespace blackjack {

// The three actions the engine and the learning agents reason about. Splitting
// is intentionally out of scope (see docs/DESIGN.md) so the MDP stays compact.
enum class Action { Stand = 0, Hit = 1, Double = 2 };

inline const char* toString(Action a) {
    switch (a) {
        case Action::Stand:  return "Stand";
        case Action::Hit:    return "Hit";
        case Action::Double: return "Double";
    }
    return "?";
}

// Compact, fully-observable state used by the tabular agents. This is the
// classic Sutton & Barto blackjack representation -- (player total, dealer
// up-card, usable ace) -- extended with `canDouble` so the agents can also
// learn *when* doubling down is worth it.
struct State {
    int  playerTotal   = 0;   // 4..21
    int  dealerUpValue = 0;   // 2..11 (11 means the dealer shows an ace)
    bool usableAce     = false;
    bool canDouble     = false; // true only on the opening two-card decision

    bool operator==(const State& o) const {
        return playerTotal == o.playerTotal &&
               dealerUpValue == o.dealerUpValue &&
               usableAce == o.usableAce &&
               canDouble == o.canDouble;
    }
};

// Hash so State can key an unordered_map. The fields are tiny, so packing them
// into one integer gives a perfect, collision-free hash.
struct StateHash {
    std::size_t operator()(const State& s) const {
        std::size_t h = static_cast<std::size_t>(s.playerTotal);
        h = h * 13u + static_cast<std::size_t>(s.dealerUpValue);
        h = h * 2u  + (s.usableAce ? 1u : 0u);
        h = h * 2u  + (s.canDouble ? 1u : 0u);
        return h;
    }
};

} // namespace blackjack
