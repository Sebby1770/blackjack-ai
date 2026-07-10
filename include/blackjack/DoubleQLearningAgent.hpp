#pragma once

#include "blackjack/Rules.hpp"
#include "blackjack/TabularAgent.hpp"

namespace blackjack {

// Double Q-Learning (Hasselt, 2010). Two independent action-value tables
// decouple action selection from evaluation so the max operator cannot
// systematically overestimate. On each transition one table is chosen at
// random to update, using the *other* table to score the greedy next action:
//
//   With p = 1/2:
//     a* = argmax_a' QA(s', a')
//     QA(s,a) <- QA(s,a) + α [ r + γ QB(s', a*) - QA(s,a) ]
//   else roles of QA / QB are swapped.
//
// Action selection (ε-greedy and the final greedy policy) uses the average of
// both tables, which is also what save() persists in the standard tabular
// format so trained policies load under any TabularAgent.
class DoubleQLearningAgent : public TabularAgent {
public:
    struct Config {
        double   alphaStart   = 0.10;
        double   alphaEnd     = 0.01;
        double   gamma        = 1.0;
        double   epsilonStart = 0.30;
        double   epsilonEnd   = 0.0;
        Rules    rules        = Rules{};
        unsigned seed         = 2024;
    };

    DoubleQLearningAgent() = default;
    explicit DoubleQLearningAgent(Config cfg) : cfg_(cfg) {}

    void train(long episodes);

    std::string name() const override { return "Double-Q-Learning"; }

    // Dual-table save/load (both QA and QB). The averaged table is always kept
    // in sync for act()/save(), so the standard TabularAgent format also works.
    bool saveDual(const std::string& path) const;
    bool loadDual(const std::string& path);

private:
    double qAValue(const State& s, Action a) const;
    double qBValue(const State& s, Action a) const;
    double avgQ(const State& s, Action a) const;
    Action greedyAvg(const State& s, const std::vector<Action>& legal) const;
    Action epsilonGreedyAvg(const State& s, const std::vector<Action>& legal,
                            double eps, std::mt19937& rng) const;
    Action argmaxA(const State& s, const std::vector<Action>& legal) const;
    Action argmaxB(const State& s, const std::vector<Action>& legal) const;
    void   syncAveraged(const State& s);

    Config cfg_;
    std::unordered_map<State, QRow, StateHash> qA_;
    std::unordered_map<State, QRow, StateHash> qB_;
};

} // namespace blackjack
