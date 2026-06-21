#include "blackjack/Chart.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace blackjack {

namespace {

char letter(Action a) {
    switch (a) {
        case Action::Stand:  return 'S';
        case Action::Hit:    return 'H';
        case Action::Double: return 'D';
    }
    return '?';
}

// Header row of dealer up-cards: 2 3 4 5 6 7 8 9 10 A.
std::string header() {
    std::ostringstream os;
    os << std::left << std::setw(6) << "" << std::right;
    const char* up[] = {"2", "3", "4", "5", "6", "7", "8", "9", "10", "A"};
    for (const char* u : up) os << std::setw(3) << u;
    return os.str();
}

// One grid: player totals `hi` down to `lo`, querying the agent for every
// dealer up-card with a fresh two-card hand (so doubles can surface).
void grid(std::ostringstream& os, const Agent& agent, int hi, int lo, bool soft) {
    const std::vector<Action> full{Action::Stand, Action::Hit, Action::Double};
    for (int t = hi; t >= lo; --t) {
        const std::string label = soft ? ("A," + std::to_string(t - 11))
                                        : std::to_string(t);
        os << std::left << std::setw(6) << label << std::right;
        for (int d = 2; d <= 11; ++d) {
            State s{t, d, soft, true};
            os << std::setw(3) << letter(agent.act(s, full));
        }
        os << "\n";
    }
}

} // namespace

std::string strategyChart(const Agent& agent) {
    std::ostringstream os;
    os << "Learned strategy -- " << agent.name()
       << "   (S = stand, H = hit, D = double)\n\n";
    os << "Hard totals\n" << header() << "\n";
    grid(os, agent, 20, 5, /*soft=*/false);
    os << "\nSoft totals (one ace counted as 11)\n" << header() << "\n";
    grid(os, agent, 20, 13, /*soft=*/true);
    return os.str();
}

} // namespace blackjack
