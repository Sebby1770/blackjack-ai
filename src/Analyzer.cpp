#include "blackjack/Analyzer.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "blackjack/CountingAgent.hpp"
#include "blackjack/Environment.hpp"

namespace blackjack {
namespace {

constexpr double P_TEN = 4.0 / 13.0;
constexpr double P_RANK = 1.0 / 13.0;

int addCard(int total, bool& soft, int v) {
    total += v;
    if (v == 11) soft = true;
    if (total > 21 && soft) {
        total -= 10;
        soft = false;
    }
    return total;
}

// pOut[0..4] = P(dealer 17..21), pOut[5] = P(bust). Memoized on (total, soft).
using Dist = std::array<double, 6>;

Dist dealerFrom(int total, bool soft, bool h17, Dist memo[22][2], char seen[22][2]) {
    if (total > 21) {
        Dist p{};
        p[5] = 1.0;
        return p;
    }
    if (total > 17 || (total == 17 && !(h17 && soft))) {
        Dist p{};
        p[static_cast<std::size_t>(total - 17)] = 1.0;
        return p;
    }
    const int idx = std::max(0, std::min(21, total));
    const int sidx = soft ? 1 : 0;
    if (seen[idx][sidx]) return memo[idx][sidx];

    Dist p{};
    auto mix = [&](int v, double w) {
        bool s = soft;
        int t = addCard(total, s, v);
        Dist q = dealerFrom(t, s, h17, memo, seen);
        for (int i = 0; i < 6; ++i) p[static_cast<std::size_t>(i)] += w * q[static_cast<std::size_t>(i)];
    };
    for (int v = 2; v <= 9; ++v) mix(v, P_RANK);
    mix(10, P_TEN);
    mix(11, P_RANK);
    seen[idx][sidx] = 1;
    memo[idx][sidx] = p;
    return p;
}

Dist dealerDist(int up, bool h17) {
    Dist memo[22][2]{};
    char seen[22][2]{};
    bool soft = (up == 11);
    return dealerFrom(up, soft, h17, memo, seen);
}

double vsDealer(int player, const std::array<double, 6>& d) {
    if (player > 21) return -1.0;
    double ev = 0.0;
    ev += d[5];                         // dealer bust => +1
    for (int t = 17; t <= 21; ++t) {
        const double pt = d[static_cast<std::size_t>(t - 17)];
        if (player > t) ev += pt;
        else if (player < t) ev -= pt;
    }
    return ev;
}

} // namespace

ActionEV infiniteDeckEV(int playerTotal, int dealerUp, bool soft,
                        const Rules& rules) {
    ActionEV ev;
    if (playerTotal < 4) playerTotal = 4;
    if (playerTotal > 21) {
        ev.stand = ev.hit = ev.doubleDown = -1.0;
        return ev;
    }
    if (dealerUp < 2) dealerUp = 2;
    if (dealerUp > 11) dealerUp = 11;

    const auto dist = dealerDist(dealerUp, rules.dealerHitsSoft17);
    ev.stand = vsDealer(playerTotal, dist);
    ev.surrender = rules.allowSurrender ? -0.5 : ev.stand;

    // One-card hit, then stand.
    double hit = 0.0;
    auto take = [&](int v, double p) {
        bool s = soft;
        int t = addCard(playerTotal, s, v);
        if (t > 21) hit += p * -1.0;
        else hit += p * vsDealer(t, dist);
    };
    for (int v = 2; v <= 9; ++v) take(v, P_RANK);
    take(10, P_TEN);
    take(11, P_RANK);
    ev.hit = hit;
    ev.doubleDown = rules.allowDouble ? (2.0 * hit) : ev.stand;
    return ev;
}

BankrollPath simulateBankroll(const CountingAgent& agent, double bankroll,
                              long hands, const Rules& rules, unsigned seed) {
    BankrollPath path;
    path.start = bankroll;
    path.minBank = bankroll;
    path.maxBank = bankroll;
    Environment env(rules, seed);
    for (long i = 0; i < hands; ++i) {
        if (bankroll < 1.0) {
            path.ruinedAt = i;
            break;
        }
        const double tc = env.trueCount();
        double unit = agent.betUnits(tc);
        if (unit > bankroll) unit = std::floor(bankroll);
        if (unit < 1.0) unit = 1.0;
        if (unit > bankroll) {
            path.ruinedAt = i;
            break;
        }

        Environment::Step step = env.reset(true);
        if (env.insuranceOffered()) {
            step = env.resolveInsurance(agent.takeInsurance(env.trueCount()));
        }
        while (!step.done) {
            const Action a = agent.decide(step.state, step.legal, env.trueCount());
            step = env.step(a);
        }
        bankroll += step.reward * unit;
        path.hands = i + 1;
        path.minBank = std::min(path.minBank, bankroll);
        path.maxBank = std::max(path.maxBank, bankroll);
    }
    path.end = bankroll;
    return path;
}

} // namespace blackjack
