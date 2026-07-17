#include "blackjack/DoubleQLearningAgent.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

#include "blackjack/Environment.hpp"

namespace blackjack {

double DoubleQLearningAgent::qAValue(const State& s, Action a) const {
    auto it = qA_.find(s);
    if (it == qA_.end()) return 0.0;
    return it->second[static_cast<int>(a)];
}

double DoubleQLearningAgent::qBValue(const State& s, Action a) const {
    auto it = qB_.find(s);
    if (it == qB_.end()) return 0.0;
    return it->second[static_cast<int>(a)];
}

double DoubleQLearningAgent::avgQ(const State& s, Action a) const {
    return 0.5 * (qAValue(s, a) + qBValue(s, a));
}

void DoubleQLearningAgent::syncAveraged(const State& s) {
    QRow& row = q_[s];
    for (int i = 0; i < 3; ++i) {
        const double a = qA_.count(s) ? qA_[s][static_cast<std::size_t>(i)] : 0.0;
        const double b = qB_.count(s) ? qB_[s][static_cast<std::size_t>(i)] : 0.0;
        row[static_cast<std::size_t>(i)] = 0.5 * (a + b);
    }
}

Action DoubleQLearningAgent::argmaxA(const State& s,
                                     const std::vector<Action>& legal) const {
    Action best = legal.empty() ? Action::Stand : legal.front();
    double bestV = -std::numeric_limits<double>::infinity();
    for (Action a : legal) {
        double v = qAValue(s, a);
        if (v > bestV) { bestV = v; best = a; }
    }
    return best;
}

Action DoubleQLearningAgent::argmaxB(const State& s,
                                     const std::vector<Action>& legal) const {
    Action best = legal.empty() ? Action::Stand : legal.front();
    double bestV = -std::numeric_limits<double>::infinity();
    for (Action a : legal) {
        double v = qBValue(s, a);
        if (v > bestV) { bestV = v; best = a; }
    }
    return best;
}

Action DoubleQLearningAgent::greedyAvg(const State& s,
                                       const std::vector<Action>& legal) const {
    Action best = legal.empty() ? Action::Stand : legal.front();
    double bestV = -std::numeric_limits<double>::infinity();
    for (Action a : legal) {
        double v = avgQ(s, a);
        if (v > bestV) { bestV = v; best = a; }
    }
    return best;
}

Action DoubleQLearningAgent::epsilonGreedyAvg(const State& s,
                                              const std::vector<Action>& legal,
                                              double eps,
                                              std::mt19937& rng) const {
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    if (coin(rng) < eps) {
        std::uniform_int_distribution<std::size_t> pick(0, legal.size() - 1);
        return legal[pick(rng)];
    }
    return greedyAvg(s, legal);
}

void DoubleQLearningAgent::train(long episodes) {
    Environment env(cfg_.rules, cfg_.seed);
    std::mt19937 rng(cfg_.seed + 3);
    std::uniform_real_distribution<double> coin(0.0, 1.0);

    for (long e = 0; e < episodes; ++e) {
        const double frac  = episodes > 1 ? static_cast<double>(e) / (episodes - 1) : 1.0;
        const double eps   = cfg_.epsilonStart +
                             (cfg_.epsilonEnd - cfg_.epsilonStart) * frac;
        const double alpha = cfg_.alphaStart +
                             (cfg_.alphaEnd - cfg_.alphaStart) * frac;

        Environment::Step s = env.reset();
        while (!s.done) {
            const State  st = s.state;
            const Action a  = epsilonGreedyAvg(st, s.legal, eps, rng);
            Environment::Step ns = env.step(a);

            const bool updateA = coin(rng) < 0.5;
            double target = ns.reward;
            if (!ns.done) {
                if (updateA) {
                    // a* = argmax QA(s'); evaluate with QB
                    const Action aStar = argmaxA(ns.state, ns.legal);
                    target += cfg_.gamma * qBValue(ns.state, aStar);
                } else {
                    const Action aStar = argmaxB(ns.state, ns.legal);
                    target += cfg_.gamma * qAValue(ns.state, aStar);
                }
            }

            if (updateA) {
                double& cell = qA_[st][static_cast<int>(a)];
                cell += alpha * (target - cell);
            } else {
                double& cell = qB_[st][static_cast<int>(a)];
                cell += alpha * (target - cell);
            }
            syncAveraged(st);

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

bool DoubleQLearningAgent::saveDual(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;
    f << "# blackjack dual-q policy: playerTotal dealerUp usableAce canDouble "
         "qAStand qAHit qADouble qBStand qBHit qBDouble\n";
    // Union of states present in either table.
    std::unordered_map<State, bool, StateHash> seen;
    for (const auto& e : qA_) seen[e.first] = true;
    for (const auto& e : qB_) seen[e.first] = true;
    for (const auto& e : seen) {
        const State& s = e.first;
        f << s.playerTotal << ' ' << s.dealerUpValue << ' '
          << (s.usableAce ? 1 : 0) << ' ' << (s.canDouble ? 1 : 0) << ' '
          << qAValue(s, Action::Stand) << ' '
          << qAValue(s, Action::Hit)   << ' '
          << qAValue(s, Action::Double) << ' '
          << qBValue(s, Action::Stand) << ' '
          << qBValue(s, Action::Hit)   << ' '
          << qBValue(s, Action::Double) << '\n';
    }
    return true;
}

bool DoubleQLearningAgent::loadDual(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    qA_.clear();
    qB_.clear();
    q_.clear();
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        State s;
        int ua = 0, cd = 0;
        QRow a{}, b{};
        if (iss >> s.playerTotal >> s.dealerUpValue >> ua >> cd
                >> a[0] >> a[1] >> a[2]
                >> b[0] >> b[1] >> b[2]) {
            s.usableAce = ua != 0;
            s.canDouble = cd != 0;
            qA_[s] = a;
            qB_[s] = b;
            syncAveraged(s);
        }
    }
    return true;
}

} // namespace blackjack
