#pragma once

#include "blackjack/Rules.hpp"
#include "blackjack/TabularAgent.hpp"

namespace blackjack {

// On-policy temporal-difference control (SARSA). Learns the action-value
// function of the *behaviour* policy, updating after every transition with
//     Q(s,a) <- Q(s,a) + alpha * [ r + gamma * Q(s',a') - Q(s,a) ]
// where a' is the action actually selected in s' (epsilon-greedy). Contrast
// with Q-Learning, which bootstraps from max_a' Q(s',a') (off-policy).
class SarsaAgent : public TabularAgent {
public:
    struct Config {
        double   alphaStart    = 0.10;
        double   alphaEnd      = 0.01;
        double   gamma         = 1.0;
        double   epsilonStart  = 0.30;
        double   epsilonEnd    = 0.0;
        Rules    rules         = Rules{};
        unsigned seed          = 2024;
        long     progressEvery = 0;  // print progress every N episodes (0 = silent)
    };

    SarsaAgent() = default;
    explicit SarsaAgent(Config cfg) : cfg_(cfg) {}

    void train(long episodes);

    std::string name() const override { return "SARSA"; }

private:
    Config cfg_;
};

} // namespace blackjack
