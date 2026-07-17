#include "blackjack/ExpectedSarsaAgent.hpp"

#include <iostream>

#include "blackjack/Environment.hpp"

namespace blackjack {

void ExpectedSarsaAgent::train(long episodes) {
    Environment env(cfg_.rules, cfg_.seed);
    std::mt19937 rng(cfg_.seed + 13);

    for (long e = 0; e < episodes; ++e) {
        const double frac  = episodes > 1 ? static_cast<double>(e) / (episodes - 1) : 1.0;
        const double eps   = cfg_.epsilonStart +
                             (cfg_.epsilonEnd - cfg_.epsilonStart) * frac;
        const double alpha = cfg_.alphaStart +
                             (cfg_.alphaEnd - cfg_.alphaStart) * frac;

        Environment::Step s = env.reset();
        while (!s.done) {
            const State  st = s.state;
            const Action a  = epsilonGreedy(st, s.legal, eps, rng);
            Environment::Step ns = env.step(a);

            double target = ns.reward;
            if (!ns.done) {
                // Expectation of Q under the current ε-greedy policy at s'.
                target += cfg_.gamma * expectedEpsilonGreedyQ(ns.state, ns.legal, eps);
            }

            double& cell = q_[st][static_cast<int>(a)];
            cell += alpha * (target - cell);

            s = ns;
        }

        if (cfg_.progressEvery > 0 &&
            ((e + 1) % cfg_.progressEvery == 0 || e + 1 == episodes)) {
            const long pct = episodes > 0 ? (100 * (e + 1)) / episodes : 100;
            std::cerr << "  [" << name() << "] progress: " << (e + 1) << "/"
                      << episodes << " (" << pct << "%)\n";
        }
    }
}

} // namespace blackjack
