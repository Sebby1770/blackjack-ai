#pragma once

#include <array>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "blackjack/Agent.hpp"

namespace blackjack {

// Shared base for agents backed by a Q-table (state -> value per action).
// It owns the table plus the greedy / epsilon-greedy policies and the
// save/load logic; subclasses only supply a training rule.
class TabularAgent : public Agent {
public:
    // Greedy policy -- the learned behaviour used during evaluation and play.
    Action act(const State& s, const std::vector<Action>& legal) const override {
        return greedy(s, legal);
    }

    bool save(const std::string& path) const;   // write the Q-table to a text file
    bool load(const std::string& path);         // restore a Q-table

    std::size_t statesLearned() const { return q_.size(); }

protected:
    using QRow = std::array<double, 4>;          // Stand, Hit, Double, Surrender

    double qValue(const State& s, Action a) const;
    double maxQ(const State& s, const std::vector<Action>& legal) const;
    // Expected action-value under an ε-greedy policy over `legal`:
    //   (1−ε)·max_a Q(s,a) + ε·mean_a Q(s,a).  Used by Expected SARSA.
    double expectedEpsilonGreedyQ(const State& s, const std::vector<Action>& legal,
                                  double eps) const;
    Action greedy(const State& s, const std::vector<Action>& legal) const;
    Action epsilonGreedy(const State& s, const std::vector<Action>& legal,
                         double eps, std::mt19937& rng) const;

    std::unordered_map<State, QRow, StateHash> q_;
};

} // namespace blackjack
