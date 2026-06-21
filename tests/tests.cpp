// Minimal dependency-free test harness. Returns non-zero on any failure so it
// plugs straight into CTest / CI.
#include <iostream>
#include <string>

#include "blackjack/BasicStrategyAgent.hpp"
#include "blackjack/Card.hpp"
#include "blackjack/Chart.hpp"
#include "blackjack/CountingAgent.hpp"
#include "blackjack/Deck.hpp"
#include "blackjack/Environment.hpp"
#include "blackjack/Game.hpp"
#include "blackjack/Hand.hpp"
#include "blackjack/MonteCarloAgent.hpp"
#include "blackjack/QLearningAgent.hpp"
#include "blackjack/RandomAgent.hpp"
#include "blackjack/Rules.hpp"

using namespace blackjack;

namespace {
int g_failures = 0;
}

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cerr << "FAIL: " << #cond << "  (line " << __LINE__ << ")\n"; \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static void testCards() {
    CHECK(Card(Rank::Ace, Suit::Spades).value() == 11);
    CHECK(Card(Rank::King, Suit::Hearts).value() == 10);
    CHECK(Card(Rank::Ten, Suit::Clubs).value() == 10);
    CHECK(Card(Rank::Seven, Suit::Diamonds).value() == 7);
    CHECK(Card(Rank::Ace, Suit::Spades).isAce());
}

static void testHandScoring() {
    Hand h;
    h.add(Card(Rank::Ace, Suit::Spades));
    h.add(Card(Rank::Six, Suit::Hearts));
    CHECK(h.value() == 17);          // soft 17
    CHECK(h.isSoft());
    h.add(Card(Rank::King, Suit::Clubs));
    CHECK(h.value() == 17);          // ace demoted to 1 -> hard 17
    CHECK(!h.isSoft());
    CHECK(!h.isBust());

    Hand bj;
    bj.add(Card(Rank::Ace, Suit::Spades));
    bj.add(Card(Rank::King, Suit::Spades));
    CHECK(bj.isBlackjack());
    CHECK(bj.value() == 21);

    Hand twoAces;
    twoAces.add(Card(Rank::Ace, Suit::Spades));
    twoAces.add(Card(Rank::Ace, Suit::Hearts));
    CHECK(twoAces.value() == 12);    // 11 + 1
    CHECK(twoAces.isSoft());

    Hand bust;
    bust.add(Card(Rank::King, Suit::Spades));
    bust.add(Card(Rank::Queen, Suit::Hearts));
    bust.add(Card(Rank::Five, Suit::Clubs));
    CHECK(bust.isBust());
    CHECK(bust.value() == 25);
}

static void testDeckAndCount() {
    Deck deck(1, 42);
    CHECK(deck.size() == 52);

    // Hi-Lo over a full, evenly-balanced single deck nets to zero.
    int dealt = 0;
    for (int i = 0; i < 52; ++i) { deck.deal(); ++dealt; }
    CHECK(dealt == 52);
    CHECK(deck.runningCount() == 0);     // 20x(+1) + 12x(0) + 20x(-1) = 0

    // Dealing past the end must auto-reshuffle (and reset the count), not crash.
    deck.deal();
    deck.deal();
}

static void testEnvironmentInvariants() {
    Environment env(Rules{}, 123);
    for (int i = 0; i < 2000; ++i) {
        Environment::Step s = env.reset();
        while (!s.done) {
            CHECK(!s.legal.empty());
            s = env.step(s.legal.front());          // always Stand
        }
        CHECK(s.reward >= -2.0 && s.reward <= 2.0);  // payoff is bounded
    }
}

static void testRulesAffectGame() {
    // No-double rules must never offer Double as a legal action.
    Rules nd; nd.allowDouble = false;
    Environment env(nd, 7);
    for (int i = 0; i < 500; ++i) {
        Environment::Step s = env.reset();
        while (!s.done) {
            for (Action a : s.legal) CHECK(a != Action::Double);
            s = env.step(Action::Hit);
        }
    }
}

static void testBasicStrategy() {
    BasicStrategyAgent bs;
    const std::vector<Action> all{Action::Stand, Action::Hit, Action::Double};
    const std::vector<Action> noDbl{Action::Stand, Action::Hit};

    CHECK(bs.act(State{20, 10, false, false}, noDbl) == Action::Stand);
    CHECK(bs.act(State{16, 10, false, false}, noDbl) == Action::Hit);
    CHECK(bs.act(State{13,  6, false, false}, noDbl) == Action::Stand);
    CHECK(bs.act(State{12,  2, false, false}, noDbl) == Action::Hit);
    CHECK(bs.act(State{12,  5, false, false}, noDbl) == Action::Stand);
    CHECK(bs.act(State{11,  6, false, true},  all)   == Action::Double);
    CHECK(bs.act(State{11,  6, false, false}, noDbl) == Action::Hit);   // can't double -> hit
    CHECK(bs.act(State{18,  6, true,  false}, noDbl) == Action::Stand); // soft 18 vs 6
}

static void testCountingAgent() {
    CountingAgent c;
    // Bet ramps up (never down) with the true count.
    CHECK(c.betUnits(-2.0) <= c.betUnits(0.0));
    CHECK(c.betUnits(0.0)  <= c.betUnits(3.0));
    CHECK(c.betUnits(5.0)  >  c.betUnits(0.0));

    const std::vector<Action> all{Action::Stand, Action::Hit, Action::Double};
    // The classic 16 vs 10 deviation: hit at a negative count, stand at a high one.
    CHECK(c.decide(State{16, 10, false, false}, {Action::Stand, Action::Hit}, -1.0) == Action::Hit);
    CHECK(c.decide(State{16, 10, false, false}, {Action::Stand, Action::Hit},  3.0) == Action::Stand);
    // Returned action is always legal.
    Action a = c.decide(State{11, 6, false, true}, all, 5.0);
    CHECK(a == Action::Stand || a == Action::Hit || a == Action::Double);
}

static void testStrategyChart() {
    BasicStrategyAgent bs;
    const std::string chart = strategyChart(bs);
    CHECK(chart.find("Hard totals") != std::string::npos);
    CHECK(chart.find("Soft totals") != std::string::npos);
    CHECK(chart.size() > 200);
}

static void testLearningBeatsRandom() {
    RandomAgent random;
    const Stats sr = evaluate(random, 20000, Rules{}, 555);

    QLearningAgent q;
    q.train(200000);
    const Stats sq = evaluate(q, 20000, Rules{}, 555);
    CHECK(q.statesLearned() > 0);
    CHECK(sq.edgePerHand() > sr.edgePerHand());     // learned policy beats random

    MonteCarloAgent mc;
    mc.train(200000);
    const Stats sm = evaluate(mc, 20000, Rules{}, 555);
    CHECK(sm.edgePerHand() > sr.edgePerHand());
}

static void testBasicStrategyNearBreakEven() {
    BasicStrategyAgent basic;
    const Stats sb = evaluate(basic, 50000, Rules{}, 555);
    CHECK(sb.edgePerHand() > -0.05);   // simplified basic strategy keeps the edge small
}

static void testCountingRuns() {
    CountingAgent counter;
    const Stats s = evaluateCounting(counter, 50000, Rules{}, 999);
    CHECK(s.hands == 50000);
    CHECK(s.avgBet() >= 1.0);                       // varying stake, at least 1 unit
    CHECK(s.edgePerHand() > -1.0 && s.edgePerHand() < 1.0);
}

static void testPolicyRoundTrip() {
    QLearningAgent q;
    q.train(50000);
    CHECK(q.save("q_roundtrip.policy"));
    QLearningAgent loaded;
    CHECK(loaded.load("q_roundtrip.policy"));
    CHECK(loaded.statesLearned() == q.statesLearned());
}

int main() {
    testCards();
    testHandScoring();
    testDeckAndCount();
    testEnvironmentInvariants();
    testRulesAffectGame();
    testBasicStrategy();
    testCountingAgent();
    testStrategyChart();
    testLearningBeatsRandom();
    testBasicStrategyNearBreakEven();
    testCountingRuns();
    testPolicyRoundTrip();

    if (g_failures == 0) {
        std::cout << "ALL TESTS PASSED\n";
        return 0;
    }
    std::cerr << g_failures << " test(s) failed\n";
    return 1;
}
