#include "blackjack/SarsaAgent.hpp"

#include <iostream>

#include "blackjack/Environment.hpp"

namespace blackjack {

void SarsaAgent::train(long episodes) {
    Environment env(cfg_.rules, cfg_.seed);
    std::mt19937 rng(cfg_.seed + 11);

    for (long e = 0; e < episodes; ++e) {
        const double frac  = episodes > 1 ? static_cast<double>(e) / (episodes - 1) : 1.0;
        const double eps   = cfg_.epsilonStart +
                             (cfg_.epsilonEnd - cfg_.epsilonStart) * frac;
        const double alpha = cfg_.alphaStart +
                             (cfg_.alphaEnd - cfg_.alphaStart) * frac;

        Environment::Step s = env.reset();
        if (s.done) {
            // Natural blackjack / instant terminal — nothing to update.
        } else {
            // Choose a0 before the loop so the SARSA (s,a,r,s',a') chain is on-policy.
            Action a = epsilonGreedy(s.state, s.legal, eps, rng);
            while (!s.done) {
                const State st = s.state;
                Environment::Step ns = env.step(a);

                double target = ns.reward;
                Action aNext = Action::Stand;
                if (!ns.done) {
                    aNext = epsilonGreedy(ns.state, ns.legal, eps, rng);
                    target += cfg_.gamma * qValue(ns.state, aNext);
                }

                double& cell = q_[st][static_cast<int>(a)];
                cell += alpha * (target - cell);

                s = ns;
                a = aNext;
            }
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
