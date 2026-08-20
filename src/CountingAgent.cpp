#include "blackjack/CountingAgent.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace blackjack {

double CountingAgent::betUnits(double tc) const {
    // The player only gains the advantage once the true count clears ~+1
    // (each +1 TC is worth roughly +0.5% in a 6-deck shoe), so the spread
    // keeps the table minimum until then and ramps hard where the edge is real.
    if (tc < 1.0) return 1.0;
    if (tc < 2.0) return 2.0;
    if (tc < 3.0) return 5.0;
    if (tc < 4.0) return 9.0;
    return 12.0;
}

std::vector<IndexPlay> CountingAgent::indexPlays() {
    // Illustrious 18 (Wong), omitting insurance (see takeInsurance) and the
    // two 10,10 pair-split indices (no split action in this engine):
    //   16 vs 10  Stand  TC >= 0
    //   15 vs 10  Stand  TC >= 4
    //   10 vs 10  Double TC >= 4
    //   12 vs 3   Stand  TC >= 2
    //   12 vs 2   Stand  TC >= 3
    //   11 vs A   Double TC >= 1
    //    9 vs 2   Double TC >= 1
    //   10 vs A   Double TC >= 4
    //    9 vs 7   Double TC >= 3
    //   16 vs 9   Stand  TC >= 5
    //   13 vs 2   Hit    TC < -1   (basic stands)
    //   12 vs 4   Hit    TC <  0   (basic stands)
    //   12 vs 5   Hit    TC < -2   (basic stands)
    //   12 vs 6   Hit    TC < -1   (basic stands)
    //   13 vs 3   Hit    TC < -2   (basic stands)
    // Fab Four (late surrender):
    //   14 vs 10  Surrender TC >= 3
    //   15 vs 9   Surrender TC >= 2
    //   15 vs 10  Surrender TC >= 0  (basic already surrenders when allowed)
    //   16 vs 9   Surrender TC >= -1 (basic already surrenders when allowed)
    return {
        {16, 10, false, Action::Stand,     0.0,  true,  "16 vs 10"},
        {15, 10, false, Action::Stand,     4.0,  true,  "15 vs 10"},
        {10, 10, false, Action::Double,    4.0,  true,  "10 vs 10"},
        {12,  3, false, Action::Stand,     2.0,  true,  "12 vs 3"},
        {12,  2, false, Action::Stand,     3.0,  true,  "12 vs 2"},
        {11, 11, false, Action::Double,    1.0,  true,  "11 vs A"},
        { 9,  2, false, Action::Double,    1.0,  true,  "9 vs 2"},
        {10, 11, false, Action::Double,    4.0,  true,  "10 vs A"},
        { 9,  7, false, Action::Double,    3.0,  true,  "9 vs 7"},
        {16,  9, false, Action::Stand,     5.0,  true,  "16 vs 9"},
        {13,  2, false, Action::Hit,      -1.0,  false, "13 vs 2"},
        {12,  4, false, Action::Hit,       0.0,  false, "12 vs 4"},
        {12,  5, false, Action::Hit,      -2.0,  false, "12 vs 5"},
        {12,  6, false, Action::Hit,      -1.0,  false, "12 vs 6"},
        {13,  3, false, Action::Hit,      -2.0,  false, "13 vs 3"},
        {14, 10, false, Action::Surrender, 3.0,  true,  "14 vs 10"},
        {15,  9, false, Action::Surrender, 2.0,  true,  "15 vs 9"},
        {15, 10, false, Action::Surrender, 0.0,  true,  "15 vs 10"},
        {16,  9, false, Action::Surrender,-1.0,  true,  "16 vs 9"},
    };
}

std::string CountingAgent::exportIndexPlays(bool json) {
    const auto plays = indexPlays();
    std::ostringstream os;
    if (json) {
        os << "[";
        bool first = true;
        for (const auto& p : plays) {
            if (!first) os << ",";
            first = false;
            os << "{\"name\":\"" << p.name << "\""
               << ",\"playerTotal\":" << p.playerTotal
               << ",\"dealerUp\":" << p.dealerUp
               << ",\"soft\":" << (p.soft ? "true" : "false")
               << ",\"action\":\"" << toString(p.action) << "\""
               << ",\"threshold\":" << p.threshold
               << ",\"atLeast\":" << (p.atLeast ? "true" : "false")
               << "}";
        }
        os << "]\n";
        return os.str();
    }

    os << "Illustrious 18 + Fab Four (no pair splits; insurance is TC >= +3)\n";
    os << std::left << std::setw(14) << "play"
       << std::setw(12) << "action"
       << "true count\n";
    for (const auto& p : plays) {
        os << std::left << std::setw(14) << p.name
           << std::setw(12) << toString(p.action)
           << (p.atLeast ? "TC >= " : "TC < ");
        const int whole = static_cast<int>(p.threshold);
        if (p.threshold == static_cast<double>(whole)) os << whole;
        else os << p.threshold;
        os << "\n";
    }
    return os.str();
}

Action CountingAgent::decide(const State& s, const std::vector<Action>& legal,
                             double tc) const {
    auto has = [&](Action a) {
        return std::find(legal.begin(), legal.end(), a) != legal.end();
    };
    const bool canDouble    = has(Action::Double);
    const bool canSurrender = has(Action::Surrender);

    auto applies = [&](const IndexPlay& p) {
        if (p.playerTotal != s.playerTotal || p.dealerUp != s.dealerUpValue ||
            p.soft != s.usableAce) {
            return false;
        }
        if (p.action == Action::Double && !canDouble) return false;
        if (p.action == Action::Surrender && !canSurrender) return false;
        return p.atLeast ? (tc >= p.threshold) : (tc < p.threshold);
    };

    // Several cells list both a surrender (Fab Four) and a stand (I18). When
    // surrender is legal it outranks stand/hit/double; otherwise the I18 play
    // fires. Loop the shared table so CLI export and decide() cannot drift.
    const auto plays = indexPlays();
    const Action order[] = {
        Action::Surrender, Action::Double, Action::Stand, Action::Hit
    };
    for (Action want : order) {
        for (const auto& p : plays) {
            if (p.action != want) continue;
            if (applies(p)) return p.action;
        }
    }

    return basic_.act(s, legal);
}

} // namespace blackjack
