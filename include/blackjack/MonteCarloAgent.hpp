#pragma once

#include "blackjack/Rules.hpp"
#include "blackjack/TabularAgent.hpp"

namespace blackjack {

// On-policy first-visit Monte Carlo control. Plays whole hands following an
// epsilon-greedy policy, then updates each visited (state, action) toward the
// observed return via an incremental sample average. Epsilon is annealed to 0
// (a GLIE schedule) so the greedy policy converges to optimal.
class MonteCarloAgent : public TabularAgent {
public:
    struct Config {
        double   epsilonStart  = 0.30;
        double   epsilonEnd    = 0.0;
        Rules    rules         = Rules{};
        unsigned seed          = 2024;
        long     progressEvery = 0;
    };

    MonteCarloAgent() = default;
    explicit MonteCarloAgent(Config cfg) : cfg_(cfg) {}

    void train(long episodes);

    std::string name() const override { return "Monte-Carlo"; }

private:
    Config cfg_;
};

} // namespace blackjack
