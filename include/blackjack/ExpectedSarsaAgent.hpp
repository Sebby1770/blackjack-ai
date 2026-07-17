#pragma once

#include "blackjack/Rules.hpp"
#include "blackjack/TabularAgent.hpp"

namespace blackjack {

// Expected SARSA: on-policy TD control that bootstraps from the *expectation*
// of Q under the behaviour policy rather than a single sampled next action:
//
//   Q(s,a) <- Q(s,a) + α [ r + γ Σ_a' π(a'|s') Q(s',a') − Q(s,a) ]
//
// For ε-greedy π this is a closed form:
//   E[Q] = (1−ε)·max_a' Q(s',a') + ε·mean_a' Q(s',a')
//
// Lower variance than SARSA; still on-policy (unlike Q-Learning's max bootstrap).
class ExpectedSarsaAgent : public TabularAgent {
public:
    struct Config {
        double   alphaStart    = 0.10;
        double   alphaEnd      = 0.01;
        double   gamma         = 1.0;
        double   epsilonStart  = 0.30;
        double   epsilonEnd    = 0.0;
        Rules    rules         = Rules{};
        unsigned seed          = 2024;
        long     progressEvery = 0;
    };

    ExpectedSarsaAgent() = default;
    explicit ExpectedSarsaAgent(Config cfg) : cfg_(cfg) {}

    void train(long episodes);

    std::string name() const override { return "Expected-SARSA"; }

private:
    Config cfg_;
};

} // namespace blackjack
