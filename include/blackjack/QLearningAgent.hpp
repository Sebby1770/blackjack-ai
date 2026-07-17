#pragma once

#include "blackjack/Rules.hpp"
#include "blackjack/TabularAgent.hpp"

namespace blackjack {

// Off-policy temporal-difference control (Q-learning). Learns the optimal
// action-value function online, updating after every card with the rule
//     Q(s,a) <- Q(s,a) + alpha * [ r + gamma * max_a' Q(s',a') - Q(s,a) ].
class QLearningAgent : public TabularAgent {
public:
    struct Config {
        double   alphaStart    = 0.10;  // learning rate at the start of training
        double   alphaEnd      = 0.01;  // learning rate at the end (annealed -> tighter convergence)
        double   gamma         = 1.0;   // discount (episodic, undiscounted)
        double   epsilonStart  = 0.30;  // exploration at the start of training
        double   epsilonEnd    = 0.0;   // exploration at the end (annealed linearly)
        Rules    rules         = Rules{};
        unsigned seed          = 2024;
        long     progressEvery = 0;     // print progress every N episodes (0 = silent)
    };

    QLearningAgent() = default;
    explicit QLearningAgent(Config cfg) : cfg_(cfg) {}

    void train(long episodes);         // self-play for `episodes` hands

    std::string name() const override { return "Q-Learning"; }

private:
    Config cfg_;
};

} // namespace blackjack
