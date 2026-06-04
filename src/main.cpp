#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "blackjack/BasicStrategyAgent.hpp"
#include "blackjack/Game.hpp"
#include "blackjack/MonteCarloAgent.hpp"
#include "blackjack/Persistence.hpp"
#include "blackjack/QLearningAgent.hpp"
#include "blackjack/RandomAgent.hpp"

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

// ----- presentation ---------------------------------------------------------

void banner() {
    std::cout <<
        "=================================================\n"
        "   Blackjack with AI  --  C++ / Reinforcement L.\n"
        "   Q-Learning | Monte Carlo | Basic Strategy\n"
        "=================================================\n";
}

void printRow(const std::string& agent, const Stats& s) {
    std::cout << std::left << std::setw(16) << agent << std::right << std::fixed
              << std::setw(7) << std::setprecision(2) << (s.winRate() * 100.0) << "%"
              << "   " << std::setw(7) << s.wins << "/" << std::setw(7) << s.losses
              << "/" << std::setw(7) << s.pushes
              << "   bank " << std::setw(9) << std::setprecision(1) << s.bankroll
              << "   edge/hand " << std::setw(8) << std::setprecision(4)
              << s.edgePerHand() << "\n";
}

// ----- sub-commands ---------------------------------------------------------

std::unique_ptr<Agent> trainAgent(const std::string& which, long episodes) {
    if (which == "mc" || which == "montecarlo") {
        auto a = std::make_unique<MonteCarloAgent>();
        std::cout << "Training Monte Carlo agent for " << episodes << " episodes...\n";
        a->train(episodes);
        std::cout << "  learned " << a->statesLearned() << " states.\n";
        return a;
    }
    auto a = std::make_unique<QLearningAgent>();
    std::cout << "Training Q-Learning agent for " << episodes << " episodes...\n";
    a->train(episodes);
    std::cout << "  learned " << a->statesLearned() << " states.\n";
    return a;
}

int cmdCompare(long episodes, long hands) {
    banner();
    std::cout << "Training agents (" << episodes << " episodes each), then "
              << "evaluating over " << hands << " hands.\n\n";

    QLearningAgent q;
    q.train(episodes);
    MonteCarloAgent mc;
    mc.train(episodes);
    BasicStrategyAgent basic;
    RandomAgent random;

    const Stats sr = evaluate(random, hands);
    const Stats sb = evaluate(basic, hands);
    const Stats sq = evaluate(q, hands);
    const Stats sm = evaluate(mc, hands);

    std::cout << "Agent              win%        W /      L /      P"
                 "        bankroll        edge/hand\n";
    std::cout << "-------------------------------------------------"
                 "--------------------------------------\n";
    printRow("Random", sr);
    printRow("Basic-Strategy", sb);
    printRow("Q-Learning", sq);
    printRow("Monte-Carlo", sm);

    StatsStore store(".");
    store.recordRun("Random", 0, sr);
    store.recordRun("Basic-Strategy", 0, sb);
    store.recordRun("Q-Learning", episodes, sq);
    store.recordRun("Monte-Carlo", episodes, sm);
    std::cout << "\nResults appended to stats.csv"
              << (StatsStore::sqlEnabled() ? " and blackjack.db (SQLite)." : ".")
              << "\n";
    std::cout << "(edge/hand is the average units won per hand; closer to 0 is "
                 "better -- the house keeps the rest.)\n";
    return 0;
}

int cmdTrain(const std::vector<std::string>& args) {
    const std::string which = args.size() > 1 ? args[1] : "q";
    const long episodes = argLong(args, "--episodes", 500000);
    const std::string out = argStr(args, "--out", "");
    auto agent = trainAgent(which, episodes);

    const Stats s = evaluate(*agent, argLong(args, "--hands", 100000));
    std::cout << agent->name() << ":  " << s.summary() << "\n";

    if (!out.empty()) {
        auto* tab = dynamic_cast<TabularAgent*>(agent.get());
        if (tab && tab->save(out)) std::cout << "Saved policy to " << out << "\n";
        else std::cout << "Could not save policy.\n";
    }
    StatsStore(".").recordRun(agent->name(), episodes, s);
    return 0;
}

int cmdEval(const std::vector<std::string>& args) {
    const std::string which = args.size() > 1 ? args[1] : "basic";
    const long hands = argLong(args, "--hands", 200000);
    const std::string in = argStr(args, "--in", "");

    std::unique_ptr<Agent> agent;
    if (which == "basic") {
        agent = std::make_unique<BasicStrategyAgent>();
    } else if (which == "random") {
        agent = std::make_unique<RandomAgent>();
    } else {
        std::unique_ptr<TabularAgent> tab =
            (which == "mc") ? std::unique_ptr<TabularAgent>(new MonteCarloAgent())
                            : std::unique_ptr<TabularAgent>(new QLearningAgent());
        if (!in.empty()) {
            if (!tab->load(in)) { std::cout << "Could not load " << in << "\n"; return 1; }
            std::cout << "Loaded " << tab->statesLearned() << " states from " << in << "\n";
        } else {
            std::cout << "No --in policy given; training a quick one (200k episodes)...\n";
            if (which == "mc") static_cast<MonteCarloAgent*>(tab.get())->train(200000);
            else               static_cast<QLearningAgent*>(tab.get())->train(200000);
        }
        agent = std::move(tab);
    }

    const Stats s = evaluate(*agent, hands);
    std::cout << agent->name() << ":  " << s.summary() << "\n";
    StatsStore(".").recordRun(agent->name(), 0, s);
    return 0;
}

int cmdWatch(const std::vector<std::string>& args) {
    const long n = argLong(args, "--hands", 5);
    std::cout << "Training a quick Q-Learning agent to watch...\n";
    QLearningAgent q;
    q.train(300000);
    watch(q, static_cast<int>(n));
    return 0;
}

int interactiveMenu() {
    std::unique_ptr<TabularAgent> trained;   // reused policy for play/watch
    BasicStrategyAgent basic;

    while (true) {
        std::cout << "\n";
        banner();
        std::cout <<
            "  1) Play Blackjack (you vs dealer, with AI hints)\n"
            "  2) Train Q-Learning agent\n"
            "  3) Train Monte Carlo agent\n"
            "  4) Compare all agents\n"
            "  5) Watch the AI play\n"
            "  6) Save / load a learned policy\n"
            "  0) Quit\n"
            "Choose: ";
        std::string line;
        if (!std::getline(std::cin, line)) return 0;
        if (line.empty()) continue;

        switch (line[0]) {
            case '1':
                playInteractive(&basic);
                break;
            case '2': {
                std::cout << "Episodes [500000]: ";
                std::string e; std::getline(std::cin, e);
                long ep = e.empty() ? 500000 : std::stol(e);
                auto q = std::make_unique<QLearningAgent>();
                q->train(ep);
                std::cout << evaluate(*q, 100000).summary() << "\n";
                trained = std::move(q);
                break;
            }
            case '3': {
                std::cout << "Episodes [500000]: ";
                std::string e; std::getline(std::cin, e);
                long ep = e.empty() ? 500000 : std::stol(e);
                auto mc = std::make_unique<MonteCarloAgent>();
                mc->train(ep);
                std::cout << evaluate(*mc, 100000).summary() << "\n";
                trained = std::move(mc);
                break;
            }
            case '4':
                cmdCompare(500000, 200000);
                break;
            case '5':
                if (trained) watch(*trained, 5);
                else cmdWatch({});
                break;
            case '6': {
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
        "                                 Train Q-Learning + Monte Carlo and\n"
        "                                 benchmark them vs basic/random\n"
        "  blackjack train q|mc [--episodes N] [--hands M] [--out FILE]\n"
        "  blackjack eval  q|mc|basic|random [--in FILE] [--hands M]\n"
        "  blackjack watch [--hands N]    Watch a trained agent play\n"
        "  blackjack demo                 Quick end-to-end smoke run\n";
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty()) return interactiveMenu();

    const std::string& cmd = args[0];
    if (cmd == "play")    { BasicStrategyAgent advisor; playInteractive(&advisor); return 0; }
    if (cmd == "compare") return cmdCompare(argLong(args, "--episodes", 500000),
                                            argLong(args, "--hands", 200000));
    if (cmd == "train")   return cmdTrain(args);
    if (cmd == "eval")    return cmdEval(args);
    if (cmd == "watch")   return cmdWatch(args);
    if (cmd == "demo")    return cmdCompare(100000, 50000);   // fast, CI-friendly
    if (cmd == "-h" || cmd == "--help" || cmd == "help") { usage(); return 0; }

    std::cout << "Unknown command: " << cmd << "\n\n";
    usage();
    return 1;
}
