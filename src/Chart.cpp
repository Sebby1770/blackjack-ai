#include "blackjack/Chart.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace blackjack {

namespace {

char letter(Action a) {
    switch (a) {
        case Action::Stand:     return 'S';
        case Action::Hit:       return 'H';
        case Action::Double:    return 'D';
        case Action::Surrender: return 'R';
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

// Self-contained HTML table with inline CSS — openable in any browser.
const char* htmlCssClass(Action a) {
    switch (a) {
        case Action::Stand:     return "S";
        case Action::Hit:       return "H";
        case Action::Double:    return "D";
        case Action::Surrender: return "R";
    }
    return "X";
}

void htmlGrid(std::ostringstream& os, const Agent& agent, int hi, int lo, bool soft) {
    const std::vector<Action> full{Action::Stand, Action::Hit, Action::Double};
    const char* up[] = {"2", "3", "4", "5", "6", "7", "8", "9", "10", "A"};
    os << "<table>\n<thead><tr><th>Hand</th>";
    for (const char* u : up) os << "<th>" << u << "</th>";
    os << "</tr></thead>\n<tbody>\n";
    for (int t = hi; t >= lo; --t) {
        const std::string label = soft ? ("A," + std::to_string(t - 11))
                                        : std::to_string(t);
        os << "<tr><th>" << label << "</th>";
        for (int d = 2; d <= 11; ++d) {
            State s{t, d, soft, true};
            const Action a = agent.act(s, full);
            os << "<td class=\"" << htmlCssClass(a) << "\">" << letter(a) << "</td>";
        }
        os << "</tr>\n";
    }
    os << "</tbody></table>\n";
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
        case ChartFormat::Html:
            os << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
                  "<meta charset=\"utf-8\">\n"
                  "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
                  "<title>Strategy chart — " << agent.name() << "</title>\n"
                  "<style>\n"
                  "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;"
                  "background:#0f1419;color:#e7ecf1;margin:0;padding:1.5rem;}\n"
                  "h1{font-size:1.35rem;margin:0 0 .25rem;}\n"
                  "h2{font-size:1.05rem;margin:1.5rem 0 .5rem;color:#9fb3c8;}\n"
                  "p.meta{color:#8a9bb0;margin:0 0 1rem;font-size:.9rem;}\n"
                  "table{border-collapse:collapse;margin:.25rem 0 1rem;"
                  "box-shadow:0 2px 12px rgba(0,0,0,.35);}\n"
                  "th,td{border:1px solid #2a3544;padding:.35rem .55rem;"
                  "text-align:center;font-weight:600;min-width:2rem;}\n"
                  "thead th,tbody th{background:#1a2330;color:#c5d4e3;font-weight:600;}\n"
                  "td.S{background:#1e4d3a;color:#b6f0d0;}\n"
                  "td.H{background:#4a2a1e;color:#ffc9a8;}\n"
                  "td.D{background:#1e3a5f;color:#a8d4ff;}\n"
                  ".legend span{display:inline-block;margin-right:.75rem;"
                  "padding:.15rem .45rem;border-radius:3px;font-size:.85rem;}\n"
                  ".legend .S{background:#1e4d3a;color:#b6f0d0;}\n"
                  ".legend .H{background:#4a2a1e;color:#ffc9a8;}\n"
                  ".legend .D{background:#1e3a5f;color:#a8d4ff;}\n"
                  "</style>\n</head>\n<body>\n"
                  "<h1>Strategy chart — " << agent.name() << "</h1>\n"
                  "<p class=\"meta\">S = stand · H = hit · D = double"
                  " &nbsp;|&nbsp; generated by blackjack-ai</p>\n"
                  "<p class=\"legend\"><span class=\"S\">S stand</span>"
                  "<span class=\"H\">H hit</span>"
                  "<span class=\"D\">D double</span></p>\n"
                  "<h2>Hard totals</h2>\n";
            htmlGrid(os, agent, 20, 5, false);
            os << "<h2>Soft totals (one ace counted as 11)</h2>\n";
            htmlGrid(os, agent, 20, 13, true);
            os << "</body>\n</html>\n";
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
