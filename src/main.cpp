#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "blackjack/BasicStrategyAgent.hpp"
#include "blackjack/Chart.hpp"
#include "blackjack/CountingAgent.hpp"
#include "blackjack/DoubleQLearningAgent.hpp"
#include "blackjack/Game.hpp"
#include "blackjack/MonteCarloAgent.hpp"
#include "blackjack/Persistence.hpp"
#include "blackjack/QLearningAgent.hpp"
#include "blackjack/RandomAgent.hpp"
#include "blackjack/Rules.hpp"

using namespace blackjack;

namespace {

constexpr const char* kVersion = "1.1.0";

// ----- tiny argument helpers ------------------------------------------------

long argLong(const std::vector<std::string>& a, const std::string& flag, long def) {
    for (std::size_t i = 0; i + 1 < a.size(); ++i)
        if (a[i] == flag) return std::stol(a[i + 1]);
    return def;
}

std::string argStr(const std::vector<std::string>& a, const std::string& flag,
                   const std::string& def) {
    for (std::size_t i = 0; i + 1 < a.size(); ++i)
        if (a[i] == flag) return a[i + 1];
    return def;
}

bool hasFlag(const std::vector<std::string>& a, const std::string& v) {
    return std::find(a.begin(), a.end(), v) != a.end();
}

double argDouble(const std::vector<std::string>& a, const std::string& flag, double def) {
    for (std::size_t i = 0; i + 1 < a.size(); ++i)
        if (a[i] == flag) return std::stod(a[i + 1]);
    return def;
}

// Build a Rules from CLI flags: --decks N, --h17, --payout X, --no-double.
Rules parseRules(const std::vector<std::string>& a) {
    Rules r;
    r.numDecks         = static_cast<int>(argLong(a, "--decks", r.numDecks));
    r.dealerHitsSoft17 = hasFlag(a, "--h17");
    r.blackjackPayout  = argDouble(a, "--payout", r.blackjackPayout);
    if (hasFlag(a, "--no-double")) r.allowDouble = false;
    return r;
}

unsigned parseSeed(const std::vector<std::string>& a, unsigned def = 2024) {
    return static_cast<unsigned>(argLong(a, "--seed", static_cast<long>(def)));
}

// ----- presentation ---------------------------------------------------------

void banner() {
    std::cout <<
        "=================================================\n"
        "   Blackjack with AI  --  C++ / Reinforcement L.\n"
        "   Q-Learning | Double-Q | Monte Carlo | Counting\n"
        "=================================================\n";
}

void rulesLine(const Rules& r) {
    std::cout << "Rules: " << r.numDecks << " decks, dealer "
              << (r.dealerHitsSoft17 ? "H17" : "S17") << ", blackjack pays "
              << r.blackjackPayout << ":1, double "
              << (r.allowDouble ? "allowed" : "off") << ".\n";
}

void printHeader() {
    std::cout << "Agent              win%        W /      L /      P"
                 "        bankroll      edge/hand   avgbet   agree%\n";
    std::cout << "-------------------------------------------------"
                 "----------------------------------------------------\n";
}

void printRow(const std::string& agent, const Stats& s, double agree = -1.0) {
    std::cout << std::left << std::setw(16) << agent << std::right << std::fixed
              << std::setw(7) << std::setprecision(2) << (s.winRate() * 100.0) << "%"
              << "   " << std::setw(7) << s.wins << "/" << std::setw(7) << s.losses
              << "/" << std::setw(7) << s.pushes
              << "   bank " << std::setw(9) << std::setprecision(1) << s.bankroll
              << "   " << std::setw(8) << std::setprecision(4) << s.edgePerHand()
              << "   " << std::setw(5) << std::setprecision(2) << s.avgBet() << "x";
    if (agree >= 0.0) {
        std::cout << "   " << std::setw(6) << std::setprecision(1) << (agree * 100.0) << "%";
    } else {
        std::cout << "       n/a";
    }
    std::cout << "\n";
}

// ----- agent construction ---------------------------------------------------

// Prepare a freshly-built tabular agent: load a saved policy if `in` is given,
// otherwise train it for `episodes` hands.
void prepareTabular(TabularAgent& agent, const std::string& in, long episodes,
                    const std::function<void()>& train) {
    if (!in.empty()) {
        if (agent.load(in))
            std::cout << "Loaded " << agent.statesLearned() << " states from " << in << "\n";
        else
            std::cout << "Could not load " << in << " (using an untrained policy).\n";
    } else {
        std::cout << "Training for " << episodes << " episodes...\n";
        train();
        std::cout << "  learned " << agent.statesLearned() << " states.\n";
    }
}

// Make an agent by name. Tabular agents are loaded from `in` if given, else
// trained for `episodes` hands under `rules` with the given seed.
std::unique_ptr<Agent> makeAgent(const std::string& which, const std::string& in,
                                 long episodes, const Rules& rules, unsigned seed) {
    if (which == "basic")  return std::make_unique<BasicStrategyAgent>();
    if (which == "random") return std::make_unique<RandomAgent>();
    if (which == "count" || which == "counter")
        return std::make_unique<CountingAgent>();

    if (which == "mc" || which == "montecarlo") {
        MonteCarloAgent::Config c; c.rules = rules; c.seed = seed;
        auto a = std::make_unique<MonteCarloAgent>(c);
        prepareTabular(*a, in, episodes, [&] { a->train(episodes); });
        return a;
    }
    if (which == "dq" || which == "doubleq" || which == "double-q") {
        DoubleQLearningAgent::Config c; c.rules = rules; c.seed = seed;
        auto a = std::make_unique<DoubleQLearningAgent>(c);
        if (!in.empty()) {
            // Prefer dual-table load when the file looks like a dual policy;
            // fall back to averaged tabular format.
            if (a->loadDual(in) && a->statesLearned() > 0) {
                std::cout << "Loaded dual-Q " << a->statesLearned()
                          << " states from " << in << "\n";
            } else {
                prepareTabular(*a, in, episodes, [&] { a->train(episodes); });
            }
        } else {
            prepareTabular(*a, "", episodes, [&] { a->train(episodes); });
        }
        return a;
    }
    // Default: single Q-Learning ("q", "ql", or unknown learner names).
    QLearningAgent::Config c; c.rules = rules; c.seed = seed;
    auto a = std::make_unique<QLearningAgent>(c);
    prepareTabular(*a, in, episodes, [&] { a->train(episodes); });
    return a;
}

bool isLearnedAgent(const std::string& which) {
    return which == "q" || which == "ql" || which == "qlearning" ||
           which == "mc" || which == "montecarlo" ||
           which == "dq" || which == "doubleq" || which == "double-q";
}

// ----- sub-commands ---------------------------------------------------------

int cmdCompare(long episodes, long hands, const Rules& rules, unsigned seed) {
    banner();
    rulesLine(rules);
    std::cout << "Seed: " << seed << "\n";
    std::cout << "Training learners (" << episodes << " episodes each), then "
              << "evaluating over " << hands << " hands.\n\n";

    QLearningAgent::Config qc; qc.rules = rules; qc.seed = seed;
    DoubleQLearningAgent::Config dqc; dqc.rules = rules; dqc.seed = seed;
    MonteCarloAgent::Config mcc; mcc.rules = rules; mcc.seed = seed;

    QLearningAgent q(qc);          q.train(episodes);
    DoubleQLearningAgent dq(dqc);  dq.train(episodes);
    MonteCarloAgent mc(mcc);       mc.train(episodes);
    BasicStrategyAgent basic;
    RandomAgent random;
    CountingAgent counter;

    // Shared eval seed so every flat-bet agent sees the same shoe sequence.
    const unsigned evalSeed = seed + 1000;
    const Stats sr = evaluate(random, hands, rules, evalSeed);
    const Stats sb = evaluate(basic, hands, rules, evalSeed);
    const Stats sq = evaluate(q, hands, rules, evalSeed);
    const Stats sd = evaluate(dq, hands, rules, evalSeed);
    const Stats sm = evaluate(mc, hands, rules, evalSeed);
    const Stats sc = evaluateCounting(counter, hands, rules, evalSeed + 1);

    const double aq = policyAgreement(q, basic);
    const double ad = policyAgreement(dq, basic);
    const double am = policyAgreement(mc, basic);
    const double ab = policyAgreement(basic, basic);

    printHeader();
    printRow("Random", sr);
    printRow("Q-Learning", sq, aq);
    printRow("Double-Q", sd, ad);
    printRow("Monte-Carlo", sm, am);
    printRow("Basic-Strategy", sb, ab);
    printRow("Card-Counter", sc);

    std::cout << std::fixed << std::setprecision(2)
              << "\nagreement_vs_basic:  Q=" << (aq * 100.0) << "%  "
              << "DQ=" << (ad * 100.0) << "%  "
              << "MC=" << (am * 100.0) << "%  "
              << "Basic=" << (ab * 100.0) << "%\n";

    StatsStore store(".");
    store.recordRun("Random", 0, sr);
    store.recordRun("Q-Learning", episodes, sq);
    store.recordRun("Double-Q-Learning", episodes, sd);
    store.recordRun("Monte-Carlo", episodes, sm);
    store.recordRun("Basic-Strategy", 0, sb);
    store.recordRun("Card-Counter", 0, sc);

    std::cout << "\nedge/hand = avg units won per hand (closer to 0 is better; "
                 "positive means the player is ahead).\n"
                 "agreement_vs_basic = % of decision states whose greedy action "
                 "matches textbook basic strategy.\n"
                 "The Card-Counter varies its bet 1x-12x with the true count, so "
                 "its bankroll reflects the\ncard-counting edge -- not a flat 1-unit game.\n";
    std::cout << "Results appended to stats.csv"
              << (StatsStore::sqlEnabled() ? " and blackjack.db (SQLite)." : ".") << "\n";
    return 0;
}

int cmdTrain(const std::vector<std::string>& args, const Rules& rules, unsigned seed) {
    const std::string which = args.size() > 1 ? args[1] : "q";
    const long episodes = argLong(args, "--episodes", 500000);
    const std::string out = argStr(args, "--out", "");
    const long hands = argLong(args, "--hands", 100000);

    auto agent = makeAgent(which, "", episodes, rules, seed);
    const Stats s = evaluate(*agent, hands, rules, seed + 1000);
    std::cout << agent->name() << ":  " << s.summary() << "\n";

    if (isLearnedAgent(which) || which == "basic") {
        BasicStrategyAgent basic;
        const double agree = policyAgreement(*agent, basic);
        std::cout << std::fixed << std::setprecision(2)
                  << "agreement_vs_basic: " << (agree * 100.0) << "%\n";
    }

    if (!out.empty()) {
        auto* tab = dynamic_cast<TabularAgent*>(agent.get());
        if (tab && tab->save(out)) {
            std::cout << "Saved policy to " << out << "\n";
            // Also write dual tables when training Double-Q and the path ends
            // with .dq (or always write a sibling .dq file).
            if (auto* dq = dynamic_cast<DoubleQLearningAgent*>(agent.get())) {
                std::string dual = out;
                if (dual.size() < 3 || dual.substr(dual.size() - 3) != ".dq")
                    dual += ".dq";
                if (dq->saveDual(dual))
                    std::cout << "Saved dual-Q tables to " << dual << "\n";
            }
        } else {
            std::cout << "Could not save policy.\n";
        }
    }
    if (hasFlag(args, "--chart")) std::cout << "\n" << strategyChart(*agent);

    StatsStore(".").recordRun(agent->name(), episodes, s);
    return 0;
}

int cmdEval(const std::vector<std::string>& args, const Rules& rules, unsigned seed) {
    const std::string which = args.size() > 1 ? args[1] : "basic";
    const long hands = argLong(args, "--hands", 200000);
    const std::string in = argStr(args, "--in", "");

    if (which == "count" || which == "counter") {
        CountingAgent counter;
        const Stats s = evaluateCounting(counter, hands, rules, seed);
        std::cout << counter.name() << ":  " << s.summary()
                  << "  avgBet=" << s.avgBet() << "x\n";
        StatsStore(".").recordRun(counter.name(), 0, s);
        return 0;
    }

    auto agent = makeAgent(which, in, 200000, rules, seed);
    const Stats s = evaluate(*agent, hands, rules, seed + 1000);
    std::cout << agent->name() << ":  " << s.summary() << "\n";

    if (isLearnedAgent(which) || which == "basic") {
        BasicStrategyAgent basic;
        const double agree = policyAgreement(*agent, basic);
        std::cout << std::fixed << std::setprecision(2)
                  << "agreement_vs_basic: " << (agree * 100.0) << "%\n";
    }

    StatsStore(".").recordRun(agent->name(), 0, s);
    return 0;
}

int cmdChart(const std::vector<std::string>& args, const Rules& rules, unsigned seed) {
    const std::string which = args.size() > 1 ? args[1] : "basic";
    const long episodes = argLong(args, "--episodes", 1000000);
    const std::string in = argStr(args, "--in", "");
    auto agent = makeAgent(which, in, episodes, rules, seed);
    std::cout << "\n" << strategyChart(*agent);
    if (isLearnedAgent(which)) {
        BasicStrategyAgent basic;
        const double agree = policyAgreement(*agent, basic);
        std::cout << std::fixed << std::setprecision(2)
                  << "\nagreement_vs_basic: " << (agree * 100.0) << "%\n";
        std::cout << "Compare against textbook play with:  blackjack chart basic\n";
    }
    return 0;
}

int cmdExportChart(const std::vector<std::string>& args, const Rules& rules,
                   unsigned seed) {
    const std::string which = args.size() > 1 ? args[1] : "basic";
    const long episodes = argLong(args, "--episodes", 500000);
    const std::string in = argStr(args, "--in", "");
    const std::string fmtStr = argStr(args, "--format", "txt");
    const std::string out = argStr(args, "--out", "");

    ChartFormat fmt = ChartFormat::Text;
    if (fmtStr == "md" || fmtStr == "markdown") fmt = ChartFormat::Markdown;
    else if (fmtStr == "csv") fmt = ChartFormat::Csv;
    else if (fmtStr == "txt" || fmtStr == "text") fmt = ChartFormat::Text;
    else {
        std::cerr << "Unknown format '" << fmtStr
                  << "' (use md, csv, or txt).\n";
        return 1;
    }

    auto agent = makeAgent(which, in, episodes, rules, seed);
    const std::string body = exportStrategyChart(*agent, fmt);

    if (out.empty()) {
        std::cout << body;
    } else {
        std::ofstream f(out);
        if (!f) {
            std::cerr << "Could not write " << out << "\n";
            return 1;
        }
        f << body;
        std::cout << "Wrote " << which << " strategy chart (" << fmtStr
                  << ") to " << out << "\n";
    }
    return 0;
}

int cmdWatch(const std::vector<std::string>& args, const Rules& rules, unsigned seed) {
    const long n = argLong(args, "--hands", 5);
    std::cout << "Training a quick Q-Learning agent to watch...\n";
    QLearningAgent::Config qc; qc.rules = rules; qc.seed = seed;
    QLearningAgent q(qc);
    q.train(300000);
    watch(q, static_cast<int>(n), rules, seed + 1000);
    return 0;
}

int cmdDemo(const Rules& rules, unsigned seed) {
    // Fast, CI-friendly smoke run that exercises every learner including Double-Q.
    banner();
    rulesLine(rules);
    std::cout << "Demo seed: " << seed << "\n";
    const long episodes = 80000;
    const long hands    = 30000;

    QLearningAgent::Config qc; qc.rules = rules; qc.seed = seed;
    DoubleQLearningAgent::Config dqc; dqc.rules = rules; dqc.seed = seed;
    MonteCarloAgent::Config mcc; mcc.rules = rules; mcc.seed = seed;

    std::cout << "Training Q / Double-Q / MC (" << episodes << " episodes)...\n";
    QLearningAgent q(qc);         q.train(episodes);
    DoubleQLearningAgent dq(dqc); dq.train(episodes);
    MonteCarloAgent mc(mcc);      mc.train(episodes);
    BasicStrategyAgent basic;

    const unsigned evalSeed = seed + 1000;
    const Stats sq = evaluate(q, hands, rules, evalSeed);
    const Stats sd = evaluate(dq, hands, rules, evalSeed);
    const Stats sm = evaluate(mc, hands, rules, evalSeed);
    const Stats sb = evaluate(basic, hands, rules, evalSeed);

    const double aq = policyAgreement(q, basic);
    const double ad = policyAgreement(dq, basic);
    const double am = policyAgreement(mc, basic);

    printHeader();
    printRow("Q-Learning", sq, aq);
    printRow("Double-Q", sd, ad);
    printRow("Monte-Carlo", sm, am);
    printRow("Basic-Strategy", sb, policyAgreement(basic, basic));
    std::cout << std::fixed << std::setprecision(2)
              << "\nagreement_vs_basic:  Q=" << (aq * 100.0) << "%  "
              << "DQ=" << (ad * 100.0) << "%  "
              << "MC=" << (am * 100.0) << "%\n";
    return 0;
}

int interactiveMenu() {
    std::unique_ptr<TabularAgent> trained;
    BasicStrategyAgent basic;
    const Rules rules;

    while (true) {
        std::cout << "\n";
        banner();
        std::cout <<
            "  1) Play Blackjack (you vs dealer, with AI hints)\n"
            "  2) Train Q-Learning agent\n"
            "  3) Train Monte Carlo agent\n"
            "  4) Train Double Q-Learning agent\n"
            "  5) Compare all agents (incl. card counter)\n"
            "  6) Watch the AI play\n"
            "  7) Show a learned strategy chart\n"
            "  8) Save / load a learned policy\n"
            "  0) Quit\n"
            "Choose: ";
        std::string line;
        if (!std::getline(std::cin, line)) return 0;
        if (line.empty()) continue;

        switch (line[0]) {
            case '1':
                playInteractive(&basic, rules);
                break;
            case '2': {
                std::cout << "Episodes [500000]: ";
                std::string e; std::getline(std::cin, e);
                long ep = e.empty() ? 500000 : std::stol(e);
                auto q = std::make_unique<QLearningAgent>();
                q->train(ep);
                std::cout << evaluate(*q, 100000, rules).summary() << "\n";
                std::cout << std::fixed << std::setprecision(2)
                          << "agreement_vs_basic: "
                          << (policyAgreement(*q, basic) * 100.0) << "%\n";
                trained = std::move(q);
                break;
            }
            case '3': {
                std::cout << "Episodes [500000]: ";
                std::string e; std::getline(std::cin, e);
                long ep = e.empty() ? 500000 : std::stol(e);
                auto mc = std::make_unique<MonteCarloAgent>();
                mc->train(ep);
                std::cout << evaluate(*mc, 100000, rules).summary() << "\n";
                std::cout << std::fixed << std::setprecision(2)
                          << "agreement_vs_basic: "
                          << (policyAgreement(*mc, basic) * 100.0) << "%\n";
                trained = std::move(mc);
                break;
            }
            case '4': {
                std::cout << "Episodes [500000]: ";
                std::string e; std::getline(std::cin, e);
                long ep = e.empty() ? 500000 : std::stol(e);
                auto dq = std::make_unique<DoubleQLearningAgent>();
                dq->train(ep);
                std::cout << evaluate(*dq, 100000, rules).summary() << "\n";
                std::cout << std::fixed << std::setprecision(2)
                          << "agreement_vs_basic: "
                          << (policyAgreement(*dq, basic) * 100.0) << "%\n";
                trained = std::move(dq);
                break;
            }
            case '5':
                cmdCompare(500000, 200000, rules, 2024);
                break;
            case '6':
                if (trained) watch(*trained, 5, rules);
                else cmdWatch({}, rules, 2024);
                break;
            case '7':
                if (trained) std::cout << "\n" << strategyChart(*trained);
                else std::cout << "Train an agent first (option 2, 3, or 4).\n";
                break;
            case '8': {
                if (!trained) {
                    std::cout << "Train an agent first (option 2, 3, or 4).\n";
                    break;
                }
                std::cout << "Filename [policy.txt]: ";
                std::string f; std::getline(std::cin, f);
                if (f.empty()) f = "policy.txt";
                std::cout << (trained->save(f) ? "Saved.\n" : "Save failed.\n");
                break;
            }
            case '0':
                return 0;
            default:
                std::cout << "Unknown option.\n";
        }
    }
}

void usage() {
    std::cout <<
        "Usage:\n"
        "  blackjack                      Launch the interactive menu\n"
        "  blackjack version              Print version (" << kVersion << ")\n"
        "  blackjack play                 Play a hand against the dealer\n"
        "  blackjack compare [--episodes N] [--hands M] [--seed S]\n"
        "                                 Train + benchmark every agent\n"
        "  blackjack train q|mc|dq [--episodes N] [--hands M] [--out FILE]\n"
        "                          [--chart] [--seed S]\n"
        "  blackjack eval  q|mc|dq|basic|random|count [--in FILE] [--hands M]\n"
        "                                              [--seed S]\n"
        "  blackjack chart q|mc|dq|basic|count [--in FILE] [--episodes N]\n"
        "                                      [--seed S]\n"
        "  blackjack export-chart basic|q|mc|dq|count [--in FILE]\n"
        "                         [--format md|csv|txt] [--out FILE]\n"
        "                         [--episodes N] [--seed S]\n"
        "  blackjack watch [--hands N] [--seed S]   Watch a trained agent play\n"
        "  blackjack demo  [--seed S]               Quick end-to-end smoke run\n"
        "\n"
        "Rule flags (any command): --decks N  --h17  --payout X  --no-double\n"
        "Reproducibility:          --seed N   (default 2024 for train/eval)\n";
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty()) return interactiveMenu();

    const Rules rules = parseRules(args);
    const unsigned seed = parseSeed(args, 2024);
    const std::string& cmd = args[0];

    if (cmd == "version" || cmd == "--version" || cmd == "-V") {
        std::cout << "blackjack " << kVersion << "\n";
        return 0;
    }
    if (cmd == "play")    { BasicStrategyAgent advisor; playInteractive(&advisor, rules, seed); return 0; }
    if (cmd == "compare") return cmdCompare(argLong(args, "--episodes", 500000),
                                            argLong(args, "--hands", 200000), rules, seed);
    if (cmd == "train")   return cmdTrain(args, rules, seed);
    if (cmd == "eval")    return cmdEval(args, rules, seed);
    if (cmd == "chart")   return cmdChart(args, rules, seed);
    if (cmd == "export-chart" || cmd == "export_chart")
        return cmdExportChart(args, rules, seed);
    if (cmd == "watch")   return cmdWatch(args, rules, seed);
    if (cmd == "demo")    return cmdDemo(rules, seed);
    if (cmd == "-h" || cmd == "--help" || cmd == "help") { usage(); return 0; }

    std::cout << "Unknown command: " << cmd << "\n\n";
    usage();
    return 1;
}
