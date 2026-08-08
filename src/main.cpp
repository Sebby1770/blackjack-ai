#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "blackjack/BasicStrategyAgent.hpp"
#include "blackjack/Chart.hpp"
#include "blackjack/CountingAgent.hpp"
#include "blackjack/DoubleQLearningAgent.hpp"
#include "blackjack/ExpectedSarsaAgent.hpp"
#include "blackjack/Game.hpp"
#include "blackjack/MonteCarloAgent.hpp"
#include "blackjack/Persistence.hpp"
#include "blackjack/QLearningAgent.hpp"
#include "blackjack/RandomAgent.hpp"
#include "blackjack/Rules.hpp"
#include "blackjack/SarsaAgent.hpp"

using namespace blackjack;

namespace {

constexpr const char* kVersion = "1.2.1";

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

// Progress interval: --progress-every N wins; else --progress => every 10%.
long parseProgressEvery(const std::vector<std::string>& a, long episodes) {
    // argLong returns def when the flag is absent; use 0 as sentinel.
    const long explicitEvery = argLong(a, "--progress-every", -1);
    if (explicitEvery > 0) return explicitEvery;
    if (hasFlag(a, "--progress")) {
        if (episodes <= 0) return 0;
        const long step = episodes / 10;
        return step > 0 ? step : 1;
    }
    return 0;
}

// ----- presentation ---------------------------------------------------------

void banner() {
    std::cout <<
        "=================================================\n"
        "   Blackjack with AI  --  C++ / Reinforcement L.\n"
        "   Q | Double-Q | SARSA | Expected-SARSA | MC | Count\n"
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

// Escape a string for JSON string values.
std::string jsonEscape(const std::string& s) {
    std::ostringstream os;
    for (char c : s) {
        switch (c) {
            case '"':  os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\n': os << "\\n";  break;
            case '\r': os << "\\r";  break;
            case '\t': os << "\\t";  break;
            default:   os << c;      break;
        }
    }
    return os.str();
}

// One agent result as a JSON object (no surrounding array).
std::string statsToJsonObject(const std::string& agent, const Stats& s,
                              long episodes, double agree = -1.0) {
    std::ostringstream os;
    os << std::fixed;
    os << "{"
       << "\"agent\":\"" << jsonEscape(agent) << "\","
       << "\"episodes\":" << episodes << ","
       << "\"hands\":" << s.hands << ","
       << "\"wins\":" << s.wins << ","
       << "\"losses\":" << s.losses << ","
       << "\"pushes\":" << s.pushes << ","
       << "\"blackjacks\":" << s.blackjacks << ","
       << std::setprecision(6)
       << "\"bankroll\":" << s.bankroll << ","
       << "\"win_rate\":" << s.winRate() << ","
       << "\"edge_per_hand\":" << s.edgePerHand() << ","
       << "\"avg_bet\":" << s.avgBet();
    if (agree >= 0.0) {
        os << ",\"agreement_vs_basic\":" << agree;
    } else {
        os << ",\"agreement_vs_basic\":null";
    }
    os << "}";
    return os.str();
}

// ----- agent construction ---------------------------------------------------

// Prepare a freshly-built tabular agent: load a saved policy if `in` is given,
// otherwise train it for `episodes` hands.
void prepareTabular(TabularAgent& agent, const std::string& in, long episodes,
                    const std::function<void()>& train, bool quiet = false) {
    if (!in.empty()) {
        if (agent.load(in)) {
            if (!quiet)
                std::cout << "Loaded " << agent.statesLearned()
                          << " states from " << in << "\n";
        } else if (!quiet) {
            std::cout << "Could not load " << in
                      << " (using an untrained policy).\n";
        }
    } else {
        if (!quiet) std::cout << "Training for " << episodes << " episodes...\n";
        train();
        if (!quiet)
            std::cout << "  learned " << agent.statesLearned() << " states.\n";
    }
}

// Make an agent by name. Tabular agents are loaded from `in` if given, else
// trained for `episodes` hands under `rules` with the given seed.
std::unique_ptr<Agent> makeAgent(const std::string& which, const std::string& in,
                                 long episodes, const Rules& rules, unsigned seed,
                                 long progressEvery = 0, bool quiet = false) {
    if (which == "basic")  return std::make_unique<BasicStrategyAgent>();
    if (which == "random") return std::make_unique<RandomAgent>();
    if (which == "count" || which == "counter")
        return std::make_unique<CountingAgent>();

    if (which == "mc" || which == "montecarlo") {
        MonteCarloAgent::Config c; c.rules = rules; c.seed = seed;
        c.progressEvery = progressEvery;
        auto a = std::make_unique<MonteCarloAgent>(c);
        prepareTabular(*a, in, episodes, [&] { a->train(episodes); }, quiet);
        return a;
    }
    if (which == "dq" || which == "doubleq" || which == "double-q") {
        DoubleQLearningAgent::Config c; c.rules = rules; c.seed = seed;
        c.progressEvery = progressEvery;
        auto a = std::make_unique<DoubleQLearningAgent>(c);
        if (!in.empty()) {
            if (a->loadDual(in) && a->statesLearned() > 0) {
                if (!quiet)
                    std::cout << "Loaded dual-Q " << a->statesLearned()
                              << " states from " << in << "\n";
            } else {
                prepareTabular(*a, in, episodes, [&] { a->train(episodes); }, quiet);
            }
        } else {
            prepareTabular(*a, "", episodes, [&] { a->train(episodes); }, quiet);
        }
        return a;
    }
    if (which == "sarsa") {
        SarsaAgent::Config c; c.rules = rules; c.seed = seed;
        c.progressEvery = progressEvery;
        auto a = std::make_unique<SarsaAgent>(c);
        prepareTabular(*a, in, episodes, [&] { a->train(episodes); }, quiet);
        return a;
    }
    if (which == "esarsa" || which == "expected-sarsa" || which == "expected_sarsa" ||
        which == "expected") {
        ExpectedSarsaAgent::Config c; c.rules = rules; c.seed = seed;
        c.progressEvery = progressEvery;
        auto a = std::make_unique<ExpectedSarsaAgent>(c);
        prepareTabular(*a, in, episodes, [&] { a->train(episodes); }, quiet);
        return a;
    }
    // Default: single Q-Learning ("q", "ql", or unknown learner names).
    QLearningAgent::Config c; c.rules = rules; c.seed = seed;
    c.progressEvery = progressEvery;
    auto a = std::make_unique<QLearningAgent>(c);
    prepareTabular(*a, in, episodes, [&] { a->train(episodes); }, quiet);
    return a;
}

bool isLearnedAgent(const std::string& which) {
    return which == "q" || which == "ql" || which == "qlearning" ||
           which == "mc" || which == "montecarlo" ||
           which == "dq" || which == "doubleq" || which == "double-q" ||
           which == "sarsa" ||
           which == "esarsa" || which == "expected-sarsa" ||
           which == "expected_sarsa" || which == "expected";
}

// ----- sub-commands ---------------------------------------------------------

int cmdCompare(long episodes, long hands, const Rules& rules, unsigned seed,
               bool jsonMode, long progressEvery) {
    if (!jsonMode) {
        banner();
        rulesLine(rules);
        std::cout << "Seed: " << seed << "\n";
        std::cout << "Training learners (" << episodes << " episodes each), then "
                  << "evaluating over " << hands << " hands.\n\n";
    }

    QLearningAgent::Config qc; qc.rules = rules; qc.seed = seed;
    qc.progressEvery = progressEvery;
    DoubleQLearningAgent::Config dqc; dqc.rules = rules; dqc.seed = seed;
    dqc.progressEvery = progressEvery;
    MonteCarloAgent::Config mcc; mcc.rules = rules; mcc.seed = seed;
    mcc.progressEvery = progressEvery;
    SarsaAgent::Config sc; sc.rules = rules; sc.seed = seed;
    sc.progressEvery = progressEvery;
    ExpectedSarsaAgent::Config esc; esc.rules = rules; esc.seed = seed;
    esc.progressEvery = progressEvery;

    if (!jsonMode) std::cout << "Training Q-Learning...\n";
    QLearningAgent q(qc);          q.train(episodes);
    if (!jsonMode) std::cout << "Training Double-Q...\n";
    DoubleQLearningAgent dq(dqc);  dq.train(episodes);
    if (!jsonMode) std::cout << "Training SARSA...\n";
    SarsaAgent sarsa(sc);          sarsa.train(episodes);
    if (!jsonMode) std::cout << "Training Expected-SARSA...\n";
    ExpectedSarsaAgent esarsa(esc); esarsa.train(episodes);
    if (!jsonMode) std::cout << "Training Monte-Carlo...\n";
    MonteCarloAgent mc(mcc);       mc.train(episodes);
    BasicStrategyAgent basic;
    RandomAgent random;
    CountingAgent counter;

    // Shared eval seed so every flat-bet agent sees the same shoe sequence.
    const unsigned evalSeed = seed + 1000;

    // Parallel evaluation: each evaluate() owns its own Environment; agents are
    // const during scoring, so independent threads are race-free.
    Stats sr, sb, sq, sd, ss, se, sm, scount;
    std::thread tRand([&] { sr = evaluate(random, hands, rules, evalSeed); });
    std::thread tBasic([&] { sb = evaluate(basic, hands, rules, evalSeed); });
    std::thread tQ([&] { sq = evaluate(q, hands, rules, evalSeed); });
    std::thread tDq([&] { sd = evaluate(dq, hands, rules, evalSeed); });
    std::thread tSarsa([&] { ss = evaluate(sarsa, hands, rules, evalSeed); });
    std::thread tEs([&] { se = evaluate(esarsa, hands, rules, evalSeed); });
    std::thread tMc([&] { sm = evaluate(mc, hands, rules, evalSeed); });
    std::thread tCount([&] {
        scount = evaluateCounting(counter, hands, rules, evalSeed + 1);
    });
    tRand.join();
    tBasic.join();
    tQ.join();
    tDq.join();
    tSarsa.join();
    tEs.join();
    tMc.join();
    tCount.join();

    const double aq = policyAgreement(q, basic);
    const double ad = policyAgreement(dq, basic);
    const double asr = policyAgreement(sarsa, basic);
    const double ae = policyAgreement(esarsa, basic);
    const double am = policyAgreement(mc, basic);
    const double ab = policyAgreement(basic, basic);

    if (jsonMode) {
        std::cout << "{"
                  << "\"version\":\"" << kVersion << "\","
                  << "\"command\":\"compare\","
                  << "\"episodes\":" << episodes << ","
                  << "\"hands\":" << hands << ","
                  << "\"seed\":" << seed << ","
                  << "\"agents\":["
                  << statsToJsonObject("Random", sr, 0)
                  << "," << statsToJsonObject("Q-Learning", sq, episodes, aq)
                  << "," << statsToJsonObject("Double-Q-Learning", sd, episodes, ad)
                  << "," << statsToJsonObject("SARSA", ss, episodes, asr)
                  << "," << statsToJsonObject("Expected-SARSA", se, episodes, ae)
                  << "," << statsToJsonObject("Monte-Carlo", sm, episodes, am)
                  << "," << statsToJsonObject("Basic-Strategy", sb, 0, ab)
                  << "," << statsToJsonObject("Card-Counter", scount, 0)
                  << "]}\n";
    } else {
        printHeader();
        printRow("Random", sr);
        printRow("Q-Learning", sq, aq);
        printRow("Double-Q", sd, ad);
        printRow("SARSA", ss, asr);
        printRow("Expected-SARSA", se, ae);
        printRow("Monte-Carlo", sm, am);
        printRow("Basic-Strategy", sb, ab);
        printRow("Card-Counter", scount);

        std::cout << std::fixed << std::setprecision(2)
                  << "\nagreement_vs_basic:  Q=" << (aq * 100.0) << "%  "
                  << "DQ=" << (ad * 100.0) << "%  "
                  << "SARSA=" << (asr * 100.0) << "%  "
                  << "ESARSA=" << (ae * 100.0) << "%  "
                  << "MC=" << (am * 100.0) << "%  "
                  << "Basic=" << (ab * 100.0) << "%\n";
    }

    StatsStore store(".");
    store.recordRun("Random", 0, sr);
    store.recordRun("Q-Learning", episodes, sq);
    store.recordRun("Double-Q-Learning", episodes, sd);
    store.recordRun("SARSA", episodes, ss);
    store.recordRun("Expected-SARSA", episodes, se);
    store.recordRun("Monte-Carlo", episodes, sm);
    store.recordRun("Basic-Strategy", 0, sb);
    store.recordRun("Card-Counter", 0, scount);

    if (!jsonMode) {
        std::cout << "\nedge/hand = avg units won per hand (closer to 0 is better; "
                     "positive means the player is ahead).\n"
                     "agreement_vs_basic = % of decision states whose greedy action "
                     "matches textbook basic strategy.\n"
                     "The Card-Counter varies its bet 1x-12x with the true count, so "
                     "its bankroll reflects the\ncard-counting edge -- not a flat 1-unit game.\n";
        std::cout << "Results appended to stats.csv"
                  << (StatsStore::sqlEnabled() ? " and blackjack.db (SQLite)." : ".")
                  << "\n";
    }
    return 0;
}

int cmdTrain(const std::vector<std::string>& args, const Rules& rules, unsigned seed) {
    const std::string which = args.size() > 1 ? args[1] : "q";
    const long episodes = argLong(args, "--episodes", 500000);
    const std::string out = argStr(args, "--out", "");
    const long hands = argLong(args, "--hands", 100000);
    const bool jsonMode = hasFlag(args, "--json");
    const long progressEvery = parseProgressEvery(args, episodes);

    auto agent = makeAgent(which, "", episodes, rules, seed, progressEvery, jsonMode);
    const Stats s = evaluate(*agent, hands, rules, seed + 1000);

    double agree = -1.0;
    if (isLearnedAgent(which) || which == "basic") {
        BasicStrategyAgent basic;
        agree = policyAgreement(*agent, basic);
    }

    if (jsonMode) {
        std::cout << "{"
                  << "\"version\":\"" << kVersion << "\","
                  << "\"command\":\"train\","
                  << "\"seed\":" << seed << ","
                  << "\"result\":"
                  << statsToJsonObject(agent->name(), s, episodes, agree)
                  << "}\n";
    } else {
        std::cout << agent->name() << ":  " << s.summary() << "\n";
        if (agree >= 0.0) {
            std::cout << std::fixed << std::setprecision(2)
                      << "agreement_vs_basic: " << (agree * 100.0) << "%\n";
        }
    }

    if (!out.empty()) {
        auto* tab = dynamic_cast<TabularAgent*>(agent.get());
        if (tab && tab->save(out)) {
            if (!jsonMode) std::cout << "Saved policy to " << out << "\n";
            if (auto* dq = dynamic_cast<DoubleQLearningAgent*>(agent.get())) {
                std::string dual = out;
                if (dual.size() < 3 || dual.substr(dual.size() - 3) != ".dq")
                    dual += ".dq";
                if (dq->saveDual(dual) && !jsonMode)
                    std::cout << "Saved dual-Q tables to " << dual << "\n";
            }
        } else if (!jsonMode) {
            std::cout << "Could not save policy.\n";
        }
    }
    if (hasFlag(args, "--chart") && !jsonMode) std::cout << "\n" << strategyChart(*agent);

    StatsStore(".").recordRun(agent->name(), episodes, s);
    return 0;
}

int cmdEval(const std::vector<std::string>& args, const Rules& rules, unsigned seed) {
    const std::string which = args.size() > 1 ? args[1] : "basic";
    const long hands = argLong(args, "--hands", 200000);
    const std::string in = argStr(args, "--in", "");
    const bool jsonMode = hasFlag(args, "--json");

    if (which == "count" || which == "counter") {
        CountingAgent counter;
        const Stats s = evaluateCounting(counter, hands, rules, seed);
        if (jsonMode) {
            std::cout << "{"
                      << "\"version\":\"" << kVersion << "\","
                      << "\"command\":\"eval\","
                      << "\"seed\":" << seed << ","
                      << "\"result\":"
                      << statsToJsonObject(counter.name(), s, 0)
                      << "}\n";
        } else {
            std::cout << counter.name() << ":  " << s.summary()
                      << "  avgBet=" << s.avgBet() << "x\n";
        }
        StatsStore(".").recordRun(counter.name(), 0, s);
        return 0;
    }

    auto agent = makeAgent(which, in, 200000, rules, seed, 0, jsonMode);
    const Stats s = evaluate(*agent, hands, rules, seed + 1000);

    double agree = -1.0;
    if (isLearnedAgent(which) || which == "basic") {
        BasicStrategyAgent basic;
        agree = policyAgreement(*agent, basic);
    }

    if (jsonMode) {
        std::cout << "{"
                  << "\"version\":\"" << kVersion << "\","
                  << "\"command\":\"eval\","
                  << "\"seed\":" << seed << ","
                  << "\"result\":"
                  << statsToJsonObject(agent->name(), s, 0, agree)
                  << "}\n";
    } else {
        std::cout << agent->name() << ":  " << s.summary() << "\n";
        if (agree >= 0.0) {
            std::cout << std::fixed << std::setprecision(2)
                      << "agreement_vs_basic: " << (agree * 100.0) << "%\n";
        }
    }

    StatsStore(".").recordRun(agent->name(), 0, s);
    return 0;
}

int cmdChart(const std::vector<std::string>& args, const Rules& rules, unsigned seed) {
    const std::string which = args.size() > 1 ? args[1] : "basic";
    const long episodes = argLong(args, "--episodes", 1000000);
    const std::string in = argStr(args, "--in", "");
    const long progressEvery = parseProgressEvery(args, episodes);
    auto agent = makeAgent(which, in, episodes, rules, seed, progressEvery);
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
    const long progressEvery = parseProgressEvery(args, episodes);

    ChartFormat fmt = ChartFormat::Text;
    if (fmtStr == "md" || fmtStr == "markdown") fmt = ChartFormat::Markdown;
    else if (fmtStr == "csv") fmt = ChartFormat::Csv;
    else if (fmtStr == "html" || fmtStr == "htm") fmt = ChartFormat::Html;
    else if (fmtStr == "txt" || fmtStr == "text") fmt = ChartFormat::Text;
    else {
        std::cerr << "Unknown format '" << fmtStr
                  << "' (use md, csv, html, or txt).\n";
        return 1;
    }

    auto agent = makeAgent(which, in, episodes, rules, seed, progressEvery);
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

int cmdDemo(const Rules& rules, unsigned seed, bool jsonMode) {
    // Fast, CI-friendly smoke run that exercises every learner.
    if (!jsonMode) {
        banner();
        rulesLine(rules);
        std::cout << "Demo seed: " << seed << "\n";
    }
    const long episodes = 80000;
    const long hands    = 30000;

    QLearningAgent::Config qc; qc.rules = rules; qc.seed = seed;
    DoubleQLearningAgent::Config dqc; dqc.rules = rules; dqc.seed = seed;
    MonteCarloAgent::Config mcc; mcc.rules = rules; mcc.seed = seed;
    SarsaAgent::Config sc; sc.rules = rules; sc.seed = seed;
    ExpectedSarsaAgent::Config esc; esc.rules = rules; esc.seed = seed;

    if (!jsonMode)
        std::cout << "Training Q / Double-Q / SARSA / Expected-SARSA / MC ("
                  << episodes << " episodes)...\n";
    QLearningAgent q(qc);         q.train(episodes);
    DoubleQLearningAgent dq(dqc); dq.train(episodes);
    SarsaAgent sarsa(sc);         sarsa.train(episodes);
    ExpectedSarsaAgent es(esc);   es.train(episodes);
    MonteCarloAgent mc(mcc);      mc.train(episodes);
    BasicStrategyAgent basic;

    const unsigned evalSeed = seed + 1000;
    Stats sq, sd, ss, se, sm, sb;
    std::thread t1([&] { sq = evaluate(q, hands, rules, evalSeed); });
    std::thread t2([&] { sd = evaluate(dq, hands, rules, evalSeed); });
    std::thread t3([&] { ss = evaluate(sarsa, hands, rules, evalSeed); });
    std::thread t4([&] { se = evaluate(es, hands, rules, evalSeed); });
    std::thread t5([&] { sm = evaluate(mc, hands, rules, evalSeed); });
    std::thread t6([&] { sb = evaluate(basic, hands, rules, evalSeed); });
    t1.join(); t2.join(); t3.join(); t4.join(); t5.join(); t6.join();

    const double aq = policyAgreement(q, basic);
    const double ad = policyAgreement(dq, basic);
    const double asr = policyAgreement(sarsa, basic);
    const double ae = policyAgreement(es, basic);
    const double am = policyAgreement(mc, basic);

    if (jsonMode) {
        std::cout << "{"
                  << "\"version\":\"" << kVersion << "\","
                  << "\"command\":\"demo\","
                  << "\"episodes\":" << episodes << ","
                  << "\"hands\":" << hands << ","
                  << "\"seed\":" << seed << ","
                  << "\"agents\":["
                  << statsToJsonObject("Q-Learning", sq, episodes, aq)
                  << "," << statsToJsonObject("Double-Q-Learning", sd, episodes, ad)
                  << "," << statsToJsonObject("SARSA", ss, episodes, asr)
                  << "," << statsToJsonObject("Expected-SARSA", se, episodes, ae)
                  << "," << statsToJsonObject("Monte-Carlo", sm, episodes, am)
                  << "," << statsToJsonObject("Basic-Strategy", sb, 0, 1.0)
                  << "]}\n";
    } else {
        printHeader();
        printRow("Q-Learning", sq, aq);
        printRow("Double-Q", sd, ad);
        printRow("SARSA", ss, asr);
        printRow("Expected-SARSA", se, ae);
        printRow("Monte-Carlo", sm, am);
        printRow("Basic-Strategy", sb, policyAgreement(basic, basic));
        std::cout << std::fixed << std::setprecision(2)
                  << "\nagreement_vs_basic:  Q=" << (aq * 100.0) << "%  "
                  << "DQ=" << (ad * 100.0) << "%  "
                  << "SARSA=" << (asr * 100.0) << "%  "
                  << "ESARSA=" << (ae * 100.0) << "%  "
                  << "MC=" << (am * 100.0) << "%\n";
    }
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
            "  5) Train SARSA agent\n"
            "  6) Train Expected SARSA agent\n"
            "  7) Compare all agents (incl. card counter)\n"
            "  8) Watch the AI play\n"
            "  9) Show a learned strategy chart\n"
            "  s) Save / load a learned policy\n"
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
            case '5': {
                std::cout << "Episodes [500000]: ";
                std::string e; std::getline(std::cin, e);
                long ep = e.empty() ? 500000 : std::stol(e);
                auto sa = std::make_unique<SarsaAgent>();
                sa->train(ep);
                std::cout << evaluate(*sa, 100000, rules).summary() << "\n";
                std::cout << std::fixed << std::setprecision(2)
                          << "agreement_vs_basic: "
                          << (policyAgreement(*sa, basic) * 100.0) << "%\n";
                trained = std::move(sa);
                break;
            }
            case '6': {
                std::cout << "Episodes [500000]: ";
                std::string e; std::getline(std::cin, e);
                long ep = e.empty() ? 500000 : std::stol(e);
                auto es = std::make_unique<ExpectedSarsaAgent>();
                es->train(ep);
                std::cout << evaluate(*es, 100000, rules).summary() << "\n";
                std::cout << std::fixed << std::setprecision(2)
                          << "agreement_vs_basic: "
                          << (policyAgreement(*es, basic) * 100.0) << "%\n";
                trained = std::move(es);
                break;
            }
            case '7':
                cmdCompare(500000, 200000, rules, 2024, false, 0);
                break;
            case '8':
                if (trained) watch(*trained, 5, rules);
                else cmdWatch({}, rules, 2024);
                break;
            case '9':
                if (trained) std::cout << "\n" << strategyChart(*trained);
                else std::cout << "Train an agent first (option 2-6).\n";
                break;
            case 's':
            case 'S': {
                if (!trained) {
                    std::cout << "Train an agent first (option 2-6).\n";
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
        "  blackjack compare [--episodes N] [--hands M] [--seed S] [--json]\n"
        "                    [--progress] [--progress-every N]\n"
        "                                 Train + benchmark every agent\n"
        "  blackjack train q|mc|dq|sarsa|esarsa [--episodes N] [--hands M]\n"
        "                          [--out FILE] [--chart] [--seed S] [--json]\n"
        "                          [--progress] [--progress-every N]\n"
        "  blackjack eval  q|mc|dq|sarsa|esarsa|basic|random|count\n"
        "                          [--in FILE] [--hands M] [--seed S] [--json]\n"
        "  blackjack chart q|mc|dq|sarsa|esarsa|basic|count [--in FILE]\n"
        "                                      [--episodes N] [--seed S]\n"
        "  blackjack export-chart basic|q|mc|dq|sarsa|esarsa|count [--in FILE]\n"
        "                         [--format md|csv|html|txt] [--out FILE]\n"
        "                         [--episodes N] [--seed S]\n"
        "  blackjack watch [--hands N] [--seed S]   Watch a trained agent play\n"
        "  blackjack demo  [--seed S] [--json]      Quick end-to-end smoke run\n"
        "\n"
        "Rule flags (any command): --decks N  --h17  --payout X  --no-double\n"
        "Reproducibility:          --seed N   (default 2024 for train/eval)\n"
        "Machine-readable:         --json     (train / eval / compare / demo)\n"
        "Training feedback:        --progress (every 10%) or --progress-every N\n";
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty()) return interactiveMenu();

    const Rules rules = parseRules(args);
    const unsigned seed = parseSeed(args, 2024);
    const std::string& cmd = args[0];
    const bool jsonMode = hasFlag(args, "--json");

    if (cmd == "version" || cmd == "--version" || cmd == "-V") {
        if (jsonMode) {
            std::cout << "{\"version\":\"" << kVersion << "\"}\n";
        } else {
            std::cout << "blackjack " << kVersion << "\n";
        }
        return 0;
    }
    if (cmd == "play")    { BasicStrategyAgent advisor; playInteractive(&advisor, rules, seed); return 0; }
    if (cmd == "compare") {
        const long episodes = argLong(args, "--episodes", 500000);
        return cmdCompare(episodes,
                          argLong(args, "--hands", 200000), rules, seed, jsonMode,
                          parseProgressEvery(args, episodes));
    }
    if (cmd == "train")   return cmdTrain(args, rules, seed);
    if (cmd == "eval")    return cmdEval(args, rules, seed);
    if (cmd == "chart")   return cmdChart(args, rules, seed);
    if (cmd == "export-chart" || cmd == "export_chart")
        return cmdExportChart(args, rules, seed);
    if (cmd == "watch")   return cmdWatch(args, rules, seed);
    if (cmd == "demo")    return cmdDemo(rules, seed, jsonMode);
    if (cmd == "-h" || cmd == "--help" || cmd == "help") { usage(); return 0; }

    std::cout << "Unknown command: " << cmd << "\n\n";
    usage();
    return 1;
}
