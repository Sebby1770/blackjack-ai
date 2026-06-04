#include "blackjack/QLearningAgent.hpp"

#include "blackjack/Environment.hpp"

namespace blackjack {

void QLearningAgent::train(long episodes) {
    Environment env(cfg_.numDecks, cfg_.seed);
    std::mt19937 rng(cfg_.seed + 1);

    for (long e = 0; e < episodes; ++e) {
        // Linearly anneal exploration from epsilonStart down to epsilonEnd.
        const double frac = episodes > 1 ? static_cast<double>(e) / (episodes - 1) : 1.0;
        const double eps  = cfg_.epsilonStart +
                            (cfg_.epsilonEnd - cfg_.epsilonStart) * frac;

        Environment::Step s = env.reset();
        while (!s.done) {
            const State  st = s.state;
            const Action a  = epsilonGreedy(st, s.legal, eps, rng);
            Environment::Step ns = env.step(a);

            // TD target: immediate reward plus the best achievable next value.
            double target = ns.reward;
            if (!ns.done) target += cfg_.gamma * maxQ(ns.state, ns.legal);

            double& cell = q_[st][static_cast<int>(a)];
            cell += cfg_.alpha * (target - cell);

            s = ns;
        }
    }
}

} // namespace blackjack
