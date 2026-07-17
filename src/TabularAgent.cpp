#include "blackjack/TabularAgent.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>

namespace blackjack {

double TabularAgent::qValue(const State& s, Action a) const {
    auto it = q_.find(s);
    if (it == q_.end()) return 0.0;                // unseen states default to 0
    return it->second[static_cast<int>(a)];
}

double TabularAgent::maxQ(const State& s, const std::vector<Action>& legal) const {
    if (legal.empty()) return 0.0;
    double best = -std::numeric_limits<double>::infinity();
    for (Action a : legal) best = std::max(best, qValue(s, a));
    return best;
}

double TabularAgent::expectedEpsilonGreedyQ(const State& s,
                                            const std::vector<Action>& legal,
                                            double eps) const {
    if (legal.empty()) return 0.0;
    double sum = 0.0;
    double best = -std::numeric_limits<double>::infinity();
    for (Action a : legal) {
        const double v = qValue(s, a);
        sum += v;
        best = std::max(best, v);
    }
    const double mean = sum / static_cast<double>(legal.size());
    // Clamp eps into [0,1] so a misconfigured schedule cannot invert the blend.
    const double e = eps < 0.0 ? 0.0 : (eps > 1.0 ? 1.0 : eps);
    return (1.0 - e) * best + e * mean;
}

Action TabularAgent::greedy(const State& s, const std::vector<Action>& legal) const {
    Action best = legal.empty() ? Action::Stand : legal.front();
    double bestV = -std::numeric_limits<double>::infinity();
    for (Action a : legal) {
        double v = qValue(s, a);
        if (v > bestV) { bestV = v; best = a; }    // ties resolve to the earlier action
    }
    return best;
}

Action TabularAgent::epsilonGreedy(const State& s, const std::vector<Action>& legal,
                                   double eps, std::mt19937& rng) const {
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    if (coin(rng) < eps) {
        std::uniform_int_distribution<std::size_t> pick(0, legal.size() - 1);
        return legal[pick(rng)];
    }
    return greedy(s, legal);
}

bool TabularAgent::save(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;
    f << "# blackjack policy: playerTotal dealerUp usableAce canDouble "
         "qStand qHit qDouble\n";
    for (const auto& entry : q_) {
        const State& s = entry.first;
        const QRow&  r = entry.second;
        f << s.playerTotal << ' ' << s.dealerUpValue << ' '
          << (s.usableAce ? 1 : 0) << ' ' << (s.canDouble ? 1 : 0) << ' '
          << r[0] << ' ' << r[1] << ' ' << r[2] << '\n';
    }
    return true;
}

bool TabularAgent::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    q_.clear();
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        State s;
        int ua = 0, cd = 0;
        QRow r{};
        if (iss >> s.playerTotal >> s.dealerUpValue >> ua >> cd
                >> r[0] >> r[1] >> r[2]) {
            s.usableAce = ua != 0;
            s.canDouble = cd != 0;
            q_[s] = r;
        }
    }
    return true;
}

} // namespace blackjack
