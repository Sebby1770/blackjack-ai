#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "blackjack/BasicStrategyAgent.hpp"
#include "blackjack/Chart.hpp"
#include "blackjack/CountingAgent.hpp"
#include "blackjack/Game.hpp"
#include "blackjack/MonteCarloAgent.hpp"
#include "blackjack/Persistence.hpp"
#include "blackjack/QLearningAgent.hpp"
#include "blackjack/RandomAgent.hpp"
#include "blackjack/Rules.hpp"

using namespace blackjack;

namespace {

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
    r.numDecks        = static_cast<int>(argLong(a, "--decks", r.numDecks));
    r.dealerHitsSoft17 = hasFlag(a, "--h17");
    r.blackjackPayout = argDouble(a, "--payout", r.blackjackPayout);
    if (hasFlag(a, "--no-double")) r.allowDouble = false;
    return r;
}

// ----- presentation ---------------------------------------------------------

void banner() {
    std::cout <<
        "=================================================\n"
        "   Blackjack with AI  --  C++ / Reinforcement L.\n"
        "   Q-Learning | Monte Carlo | Counting | Basic\n"
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
                 "        bankroll      edge/hand   avgbet\n";
    std::cout << "-------------------------------------------------"
                 "-------------------------------------------\n";
}

void printRow(const std::string& agent, const Stats& s) {
    std::cout << std::left << std::setw(16) << agent << std::right << std::fixed
              << std::setw(7) << std::setprecision(2) << (s.winRate() * 100.0) << "%"
              << "   " << std::setw(7) << s.wins << "/" << std::setw(7) << s.losses
              << "/" << std::setw(7) << s.pushes
              << "   bank " << std::setw(9) << std::setprecision(1) << s.bankroll
              << "   " << std::setw(8) << std::setprecision(4) << s.edgePerHand()
              << "   " << std::setw(5) << std::setprecision(2) << s.avgBet() << "x\n";
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
// trained for `episodes` hands under `rules`.
std::unique_ptr<Agent> makeAgent(const std::string& which, const std::string& in,
                                 long episodes, const Rules& rules) {
    if (which == "basic")  return std::make_unique<BasicStrategyAgent>();
    if (which == "random") return std::make_unique<RandomAgent>();
    if (which == "count" || which == "counter")
        return std::make_unique<CountingAgent>();

    if (which == "mc" || which == "montecarlo") {
        MonteCarloAgent::Config c; c.rules = rules;
        auto a = std::make_unique<MonteCarloAgent>(c);
        prepareTabular(*a, in, episodes, [&] { a->train(episodes); });
        return a;
    }
    QLearningAgent::Config c; c.rules = rules;
    auto a = std::make_unique<QLearningAgent>(c);
    prepareTabular(*a, in, episodes, [&] { a->train(episodes); });
    return a;
}

// ----- sub-commands ---------------------------------------------------------

int cmdCompare(long episodes, long hands, const Rules& rules) {
    banner();
    rulesLine(rules);
    std::cout << "Training learners (" << episodes << " episodes each), then "
              << "evaluating over " << hands << " hands.\n\n";

    QLearningAgent::Config qc; qc.rules = rules;
    MonteCarloAgent::Config mcc; mcc.rules = rules;
    QLearningAgent q(qc);   q.train(episodes);
    MonteCarloAgent mc(mcc); mc.train(episodes);
    BasicStrategyAgent basic;
    RandomAgent random;
    CountingAgent counter;

    const Stats sr = evaluate(random, hands, rules);
    const Stats sb = evaluate(basic, hands, rules);
    const Stats sq = evaluate(q, hands, rules);
    const Stats sm = evaluate(mc, hands, rules);
    const Stats sc = evaluateCounting(counter, hands, rules);

    printHeader();
    printRow("Random", sr);
    printRow("Q-Learning", sq);
    printRow("Monte-Carlo", sm);
    printRow("Basic-Strategy", sb);
    printRow("Card-Counter", sc);

    StatsStore store(".");
    store.recordRun("Random", 0, sr);
    store.recordRun("Q-Learning", episodes, sq);
    store.recordRun("Monte-Carlo", episodes, sm);
    store.recordRun("Basic-Strategy", 0, sb);
    store.recordRun("Card-Counter", 0, sc);

    std::cout << "\nedge/hand = avg units won per hand (closer to 0 is better; "
                 "positive means the player is ahead).\n"
                 "The Card-Counter varies its bet 1x-12x with the true count, so "
                 "its bankroll reflects the\ncard-counting edge -- not a flat 1-unit game.\n";
    std::cout << "Results appended to stats.csv"
              << (StatsStore::sqlEnabled() ? " and blackjack.db (SQLite)." : ".") << "\n";
    return 0;
}

int cmdTrain(const std::vector<std::string>& args, const Rules& rules) {
    const std::string which = args.size() > 1 ? args[1] : "q";
    const long episodes = argLong(args, "--episodes", 500000);
    const std::string out = argStr(args, "--out", "");

    auto agent = makeAgent(which, "", episodes, rules);
    const Stats s = evaluate(*agent, argLong(args, "--hands", 100000), rules);
    std::cout << agent->name() << ":  " << s.summary() << "\n";

    if (!out.empty()) {
        auto* tab = dynamic_cast<TabularAgent*>(agent.get());
        if (tab && tab->save(out)) std::cout << "Saved policy to " << out << "\n";
        else std::cout << "Could not save policy.\n";
    }
    if (hasFlag(args, "--chart")) std::cout << "\n" << strategyChart(*agent);

    StatsStore(".").recordRun(agent->name(), episodes, s);
    return 0;
}

int cmdEval(const std::vector<std::string>& args, const Rules& rules) {
    const std::string which = args.size() > 1 ? args[1] : "basic";
    const long hands = argLong(args, "--hands", 200000);
    const std::string in = argStr(args, "--in", "");

    if (which == "count" || which == "counter") {
        CountingAgent counter;
        const Stats s = evaluateCounting(counter, hands, rules);
        std::cout << counter.name() << ":  " << s.summary()
                  << "  avgBet=" << s.avgBet() << "x\n";
        StatsStore(".").recordRun(counter.name(), 0, s);
        return 0;
    }

    auto agent = makeAgent(which, in, 200000, rules);
    const Stats s = evaluate(*agent, hands, rules);
    std::cout << agent->name() << ":  " << s.summary() << "\n";
    StatsStore(".").recordRun(agent->name(), 0, s);
    return 0;
}

int cmdChart(const std::vector<std::string>& args, const Rules& rules) {
    const std::string which = args.size() > 1 ? args[1] : "basic";
    const long episodes = argLong(args, "--episodes", 1000000);
    const std::string in = argStr(args, "--in", "");
    auto agent = makeAgent(which, in, episodes, rules);
    std::cout << "\n" << strategyChart(*agent);
    if (which != "basic" && which != "random" && which != "count")
        std::cout << "\nCompare against textbook play with:  blackjack chart basic\n";
    return 0;
}

int cmdWatch(const std::vector<std::string>& args, const Rules& rules) {
    const long n = argLong(args, "--hands", 5);
    std::cout << "Training a quick Q-Learning agent to watch...\n";
    QLearningAgent::Config qc; qc.rules = rules;
    QLearningAgent q(qc);
    q.train(300000);
    watch(q, static_cast<int>(n), rules);
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
            "  4) Compare all agents (incl. card counter)\n"
            "  5) Watch the AI play\n"
            "  6) Show a learned strategy chart\n"
            "  7) Save / load a learned policy\n"
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
                trained = std::move(mc);
                break;
            }
            case '4':
                cmdCompare(500000, 200000, rules);
                break;
            case '5':
                if (trained) watch(*trained, 5, rules);
                else cmdWatch({}, rules);
                break;
            case '6':
                if (trained) std::cout << "\n" << strategyChart(*trained);
                else std::cout << "Train an agent first (option 2 or 3).\n";
                break;
            case '7': {
                if (!trained) { std::cout << "Train an agent first (option 2 or 3).\n"; break; }
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
        "  blackjack play                 Play a hand against the dealer\n"
        "  blackjack compare [--episodes N] [--hands M]\n"
        "                                 Train + benchmark every agent\n"
        "  blackjack train q|mc [--episodes N] [--hands M] [--out FILE] [--chart]\n"
        "  blackjack eval  q|mc|basic|random|count [--in FILE] [--hands M]\n"
        "  blackjack chart q|mc|basic|count [--in FILE] [--episodes N]\n"
        "  blackjack watch [--hands N]    Watch a trained agent play\n"
        "  blackjack demo                 Quick end-to-end smoke run\n"
        "\n"
        "Rule flags (any command): --decks N  --h17  --payout X  --no-double\n";
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty()) return interactiveMenu();

    const Rules rules = parseRules(args);
    const std::string& cmd = args[0];

    if (cmd == "play")    { BasicStrategyAgent advisor; playInteractive(&advisor, rules); return 0; }
    if (cmd == "compare") return cmdCompare(argLong(args, "--episodes", 500000),
                                            argLong(args, "--hands", 200000), rules);
    if (cmd == "train")   return cmdTrain(args, rules);
    if (cmd == "eval")    return cmdEval(args, rules);
    if (cmd == "chart")   return cmdChart(args, rules);
    if (cmd == "watch")   return cmdWatch(args, rules);
    if (cmd == "demo")    return cmdCompare(100000, 50000, rules);   // fast, CI-friendly
    if (cmd == "-h" || cmd == "--help" || cmd == "help") { usage(); return 0; }

    std::cout << "Unknown command: " << cmd << "\n\n";
    usage();
    return 1;
}
