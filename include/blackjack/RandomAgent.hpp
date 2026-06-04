#pragma once

#include <random>

#include "blackjack/Agent.hpp"

namespace blackjack {

// Picks uniformly among the legal actions. Serves as the lower-bound baseline:
// any agent that has learned anything should comfortably beat it.
class RandomAgent : public Agent {
public:
    explicit RandomAgent(unsigned seed = 99) : rng_(seed) {}

    Action act(const State&, const std::vector<Action>& legal) const override {
        std::uniform_int_distribution<std::size_t> pick(0, legal.size() - 1);
        return legal[pick(rng_)];
    }

    std::string name() const override { return "Random"; }

private:
    mutable std::mt19937 rng_;
};

} // namespace blackjack
