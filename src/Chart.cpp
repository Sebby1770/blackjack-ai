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

// Legal actions for a state: Stand + Hit always; Double only when canDouble.
std::vector<Action> legalFor(const State& s) {
    std::vector<Action> legal{Action::Stand, Action::Hit};
    if (s.canDouble) legal.push_back(Action::Double);
    return legal;
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

void mdHeader(std::ostringstream& os) {
    os << "| hand | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | A |\n";
    os << "|------|---|---|---|---|---|---|---|---|----|---|\n";
}

void mdGrid(std::ostringstream& os, const Agent& agent, int hi, int lo, bool soft) {
    const std::vector<Action> full{Action::Stand, Action::Hit, Action::Double};
    for (int t = hi; t >= lo; --t) {
        const std::string label = soft ? ("A," + std::to_string(t - 11))
                                        : std::to_string(t);
        os << "| " << label;
        for (int d = 2; d <= 11; ++d) {
            State s{t, d, soft, true};
            os << " | " << letter(agent.act(s, full));
        }
        os << " |\n";
    }
}

void csvHeader(std::ostringstream& os) {
    os << "section,player,d2,d3,d4,d5,d6,d7,d8,d9,d10,dA\n";
}

void csvGrid(std::ostringstream& os, const Agent& agent, const char* section,
             int hi, int lo, bool soft) {
    const std::vector<Action> full{Action::Stand, Action::Hit, Action::Double};
    for (int t = hi; t >= lo; --t) {
        const std::string label = soft ? ("A," + std::to_string(t - 11))
                                        : std::to_string(t);
        os << section << ',' << label;
        for (int d = 2; d <= 11; ++d) {
            State s{t, d, soft, true};
            os << ',' << letter(agent.act(s, full));
        }
        os << '\n';
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

std::string exportStrategyChart(const Agent& agent, ChartFormat fmt) {
    std::ostringstream os;
    switch (fmt) {
        case ChartFormat::Markdown:
            os << "# Strategy chart -- " << agent.name() << "\n\n";
            os << "S = stand, H = hit, D = double\n\n";
            os << "## Hard totals\n\n";
            mdHeader(os);
            mdGrid(os, agent, 20, 5, false);
            os << "\n## Soft totals\n\n";
            mdHeader(os);
            mdGrid(os, agent, 20, 13, true);
            break;
        case ChartFormat::Csv:
            csvHeader(os);
            csvGrid(os, agent, "hard", 20, 5, false);
            csvGrid(os, agent, "soft", 20, 13, true);
            break;
        case ChartFormat::Text:
        default:
            return strategyChart(agent);
    }
    return os.str();
}

double policyAgreement(const Agent& learned, const BasicStrategyAgent& basic) {
    long agree = 0;
    long total = 0;

    // Walk hard totals 4-21 × dealer 2-A × soft/hard × canDouble.
    // Soft hands with total < 12 are impossible (A+A = soft 12), so soft
    // starts at 12. Hard starts at 4 (lowest two-card total: 2+2).
    for (bool soft : {false, true}) {
        const int lo = soft ? 12 : 4;
        for (int total_pt = lo; total_pt <= 21; ++total_pt) {
            for (int d = 2; d <= 11; ++d) {
                for (bool canDbl : {false, true}) {
                    State s{total_pt, d, soft, canDbl};
                    const auto legal = legalFor(s);
                    const Action aL = learned.act(s, legal);
                    const Action aB = basic.act(s, legal);
                    if (aL == aB) ++agree;
                    ++total;
                }
            }
        }
    }
    if (total == 0) return 1.0;
    return static_cast<double>(agree) / static_cast<double>(total);
}

} // namespace blackjack
