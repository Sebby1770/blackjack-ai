#include "blackjack/MonteCarloAgent.hpp"

#include <array>
#include <unordered_map>
#include <vector>

#include "blackjack/Environment.hpp"

namespace blackjack {

void MonteCarloAgent::train(long episodes) {
    Environment env(cfg_.numDecks, cfg_.seed);
    std::mt19937 rng(cfg_.seed + 7);

    // Visit counts drive the incremental sample-average update.
    std::unordered_map<State, std::array<long, 3>, StateHash> counts;

    struct SA { State s; Action a; };
    std::vector<SA> trajectory;

    for (long e = 0; e < episodes; ++e) {
        const double frac = episodes > 1 ? static_cast<double>(e) / (episodes - 1) : 1.0;
        const double eps  = cfg_.epsilonStart +
                            (cfg_.epsilonEnd - cfg_.epsilonStart) * frac;

        trajectory.clear();
        Environment::Step s = env.reset();
        double G = s.reward;                 // a natural ends the hand immediately
        while (!s.done) {
            const Action a = epsilonGreedy(s.state, s.legal, eps, rng);
            trajectory.push_back({s.state, a});
            s = env.step(a);
            G = s.reward;                    // gamma = 1 and only the terminal reward
        }                                    // is non-zero, so every return equals G.

        // First-visit Monte Carlo update.
        for (std::size_t i = 0; i < trajectory.size(); ++i) {
            bool firstVisit = true;
            for (std::size_t j = 0; j < i; ++j) {
                if (trajectory[j].s == trajectory[i].s &&
                    trajectory[j].a == trajectory[i].a) {
                    firstVisit = false;
                    break;
                }
            }
            if (!firstVisit) continue;

            const int ai = static_cast<int>(trajectory[i].a);
            const long n = ++counts[trajectory[i].s][ai];
            double& cell = q_[trajectory[i].s][ai];
            cell += (G - cell) / static_cast<double>(n);
        }
    }
}

} // namespace blackjack
