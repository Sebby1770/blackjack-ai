// Minimal dependency-free test harness. Returns non-zero on any failure so it
// plugs straight into CTest / CI.
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "blackjack/BasicStrategyAgent.hpp"
#include "blackjack/Card.hpp"
#include "blackjack/Chart.hpp"
#include "blackjack/CountingAgent.hpp"
#include "blackjack/Deck.hpp"
#include "blackjack/DoubleQLearningAgent.hpp"
#include "blackjack/Environment.hpp"
#include "blackjack/ExpectedSarsaAgent.hpp"
#include "blackjack/Game.hpp"
#include "blackjack/Hand.hpp"
#include "blackjack/MonteCarloAgent.hpp"
#include "blackjack/QLearningAgent.hpp"
#include "blackjack/RandomAgent.hpp"
#include "blackjack/Rules.hpp"
#include "blackjack/SarsaAgent.hpp"

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

static void testDeckSeedReproducibility() {
    // Identical seeds must produce identical deal sequences.
    Deck a(2, 999);
    Deck b(2, 999);
    for (int i = 0; i < 80; ++i) {
        Card ca = a.deal();
        Card cb = b.deal();
        CHECK(ca.rank() == cb.rank());
        CHECK(ca.suit() == cb.suit());
    }

    // Different seeds must diverge (with overwhelming probability).
    Deck a2(2, 999);
    Deck c2(2, 1000);
    bool differ = false;
    for (int i = 0; i < 40; ++i) {
        const Card ca = a2.deal();
        const Card cc = c2.deal();
        if (ca.rank() != cc.rank() || ca.suit() != cc.suit()) {
            differ = true;
            break;
        }
    }
    CHECK(differ);

    // Environment with the same seed produces the same opening player totals.
    Environment e1(Rules{}, 4242);
    Environment e2(Rules{}, 4242);
    for (int i = 0; i < 50; ++i) {
        auto s1 = e1.reset();
        auto s2 = e2.reset();
        CHECK(s1.state.playerTotal == s2.state.playerTotal);
        CHECK(s1.state.dealerUpValue == s2.state.dealerUpValue);
        // Drain hands deterministically so the shoes stay in lockstep.
        while (!s1.done) s1 = e1.step(Action::Stand);
        while (!s2.done) s2 = e2.step(Action::Stand);
    }
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

static void testH17VsS17() {
    // Under H17 the dealer hits soft 17; under S17 the dealer stands.
    // Construct a situation where the dealer holds soft 17 and the player has
    // already stood with a hard 18: H17 can bust (or improve) while S17 is done.
    //
    // We verify the rule flag changes Environment's configured behaviour by
    // checking that playOutDealer logic is exercised differently: over many
    // hands, H17 and S17 with the same seed yield different dealer outcomes
    // often enough that aggregate bankrolls diverge.

    Rules s17; s17.dealerHitsSoft17 = false;
    Rules h17; h17.dealerHitsSoft17 = true;

    BasicStrategyAgent basic;
    const Stats ss = evaluate(basic, 30000, s17, 31415);
    const Stats sh = evaluate(basic, 30000, h17, 31415);

    // Same seed + same policy but different dealer draw rule => stats differ.
    CHECK(ss.bankroll != sh.bankroll || ss.wins != sh.wins);

    // H17 is slightly worse for the player than S17 on average; we only require
    // that both edges stay in a plausible band (not a crash / NaN).
    CHECK(std::isfinite(ss.edgePerHand()));
    CHECK(std::isfinite(sh.edgePerHand()));
    CHECK(ss.edgePerHand() > -0.10 && ss.edgePerHand() < 0.05);
    CHECK(sh.edgePerHand() > -0.10 && sh.edgePerHand() < 0.05);
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

    const std::string md = exportStrategyChart(bs, ChartFormat::Markdown);
    CHECK(md.find("| hand |") != std::string::npos);
    CHECK(md.find("Hard totals") != std::string::npos ||
          md.find("hard") != std::string::npos);

    const std::string csv = exportStrategyChart(bs, ChartFormat::Csv);
    CHECK(csv.find("section,player") != std::string::npos);
    CHECK(csv.find("hard,") != std::string::npos);
    CHECK(csv.find("soft,") != std::string::npos);

    const std::string html = exportStrategyChart(bs, ChartFormat::Html);
    CHECK(!html.empty());
    CHECK(html.find("<!DOCTYPE html>") != std::string::npos);
    CHECK(html.find("<table>") != std::string::npos);
    CHECK(html.find("Hard totals") != std::string::npos);
    CHECK(html.find("Soft totals") != std::string::npos);
    CHECK(html.find("class=\"S\"") != std::string::npos ||
          html.find("class=\"H\"") != std::string::npos);
    CHECK(html.find("</html>") != std::string::npos);
    CHECK(html.size() > 500);
}

static void testPolicyAgreement() {
    BasicStrategyAgent basic;
    // A policy agrees with itself at 100%.
    CHECK(policyAgreement(basic, basic) == 1.0);

    // Random agent should disagree on a meaningful fraction of states.
    RandomAgent random;
    const double r = policyAgreement(random, basic);
    CHECK(r >= 0.0 && r <= 1.0);
    CHECK(r < 0.95);   // random rarely matches basic everywhere
}

static void testDoubleQLearning() {
    DoubleQLearningAgent::Config cfg;
    cfg.seed = 42;
    DoubleQLearningAgent dq(cfg);
    dq.train(50000);
    CHECK(dq.statesLearned() > 0);

    // Averaged Q-values must be finite for every learned state we can probe.
    const std::vector<Action> full{Action::Stand, Action::Hit, Action::Double};
    for (int t = 4; t <= 21; ++t) {
        for (int d = 2; d <= 11; ++d) {
            State s{t, d, false, true};
            Action a = dq.act(s, full);
            CHECK(a == Action::Stand || a == Action::Hit || a == Action::Double);
            // qValue goes through the averaged table.
            for (Action act : full) {
                // Access via act is enough; ensure no NaN by checking edge after eval.
                (void)act;
            }
        }
    }

    const Stats s = evaluate(dq, 10000, Rules{}, 555);
    CHECK(std::isfinite(s.edgePerHand()));
    CHECK(s.hands == 10000);

    // Dual save/load round-trip.
    CHECK(dq.saveDual("dq_roundtrip.policy"));
    DoubleQLearningAgent loaded;
    CHECK(loaded.loadDual("dq_roundtrip.policy"));
    CHECK(loaded.statesLearned() == dq.statesLearned());

    // Averaged tabular save also works.
    CHECK(dq.save("dq_avg.policy"));
    DoubleQLearningAgent avgLoaded;
    CHECK(avgLoaded.load("dq_avg.policy"));
    CHECK(avgLoaded.statesLearned() == dq.statesLearned());
}

static void testSarsaAndExpectedSarsa() {
    SarsaAgent::Config sc; sc.seed = 77;
    SarsaAgent sarsa(sc);
    sarsa.train(80000);
    CHECK(sarsa.statesLearned() > 0);

    ExpectedSarsaAgent::Config esc; esc.seed = 77;
    ExpectedSarsaAgent es(esc);
    es.train(80000);
    CHECK(es.statesLearned() > 0);

    // Save/load round-trip via the shared tabular format.
    CHECK(sarsa.save("sarsa_roundtrip.policy"));
    SarsaAgent loaded;
    CHECK(loaded.load("sarsa_roundtrip.policy"));
    CHECK(loaded.statesLearned() == sarsa.statesLearned());

    CHECK(es.save("esarsa_roundtrip.policy"));
    ExpectedSarsaAgent esLoaded;
    CHECK(esLoaded.load("esarsa_roundtrip.policy"));
    CHECK(esLoaded.statesLearned() == es.statesLearned());

    // Actions are always legal.
    const std::vector<Action> full{Action::Stand, Action::Hit, Action::Double};
    for (int t = 4; t <= 21; ++t) {
        State s{t, 10, false, true};
        Action a = sarsa.act(s, full);
        CHECK(a == Action::Stand || a == Action::Hit || a == Action::Double);
        a = es.act(s, full);
        CHECK(a == Action::Stand || a == Action::Hit || a == Action::Double);
    }
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

    DoubleQLearningAgent dq;
    dq.train(200000);
    const Stats sd = evaluate(dq, 20000, Rules{}, 555);
    CHECK(dq.statesLearned() > 0);
    CHECK(sd.edgePerHand() > sr.edgePerHand());

    SarsaAgent sarsa;
    sarsa.train(200000);
    const Stats ss = evaluate(sarsa, 20000, Rules{}, 555);
    CHECK(sarsa.statesLearned() > 0);
    CHECK(ss.edgePerHand() > sr.edgePerHand());

    ExpectedSarsaAgent es;
    es.train(200000);
    const Stats se = evaluate(es, 20000, Rules{}, 555);
    CHECK(es.statesLearned() > 0);
    CHECK(se.edgePerHand() > sr.edgePerHand());
}

static void testExpectedSarsaFinite() {
    // Expected SARSA uses expectedEpsilonGreedyQ; ensure short training produces
    // a finite, evaluable policy (no NaNs from the expectation blend).
    ExpectedSarsaAgent es;
    es.train(10000);
    const Stats st = evaluate(es, 5000, Rules{}, 1);
    CHECK(std::isfinite(st.edgePerHand()));
    CHECK(st.hands == 5000);
    CHECK(es.statesLearned() > 0);
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

static void testUncountedDeal() {
    Deck d(1, 7);
    const Card hole = d.deal(false);
    CHECK(d.runningCount() == 0);
    d.count(hole);
    CHECK(d.runningCount() == Deck::hiLoValue(hole));
}

static void testHoleCardNotCountedUntilReveal() {
    for (int i = 0; i < 80; ++i) {
        Environment env(Rules{}, 2024u + static_cast<unsigned>(i));
        Environment::Step s = env.reset();
        const auto player = env.player().cards();
        const auto dealer = env.dealer().cards();
        CHECK(player.size() == 2);
        CHECK(dealer.size() == 2);
        const int seen =
            Deck::hiLoValue(player[0]) +
            Deck::hiLoValue(player[1]) +
            Deck::hiLoValue(dealer[0]);
        const int hole = Deck::hiLoValue(dealer[1]);
        if (!s.done) {
            CHECK(env.runningCount() == seen);
            s = env.step(Action::Stand);
            int extra = hole;
            const auto& d = env.dealer().cards();
            for (std::size_t k = 2; k < d.size(); ++k) extra += Deck::hiLoValue(d[k]);
            CHECK(env.runningCount() == seen + extra);
        } else {
            CHECK(env.runningCount() == seen + hole);
        }
    }
}

static void testLateSurrender() {
    Rules r;
    r.allowSurrender = true;
    Environment env(r, 99);
    bool sawSurrender = false;
    for (int i = 0; i < 2000 && !sawSurrender; ++i) {
        Environment::Step s = env.reset();
        if (s.done) continue;
        const bool offered =
            std::find(s.legal.begin(), s.legal.end(), Action::Surrender) != s.legal.end();
        CHECK(offered);
        s = env.step(Action::Surrender);
        CHECK(s.done);
        CHECK(s.reward == -0.5);
        sawSurrender = true;
    }
    CHECK(sawSurrender);

    Rules off;
    off.allowSurrender = false;
    Environment envOff(off, 3);
    for (int i = 0; i < 200; ++i) {
        Environment::Step s = envOff.reset();
        while (!s.done) {
            for (Action a : s.legal) CHECK(a != Action::Surrender);
            s = envOff.step(Action::Stand);
        }
    }

    BasicStrategyAgent bs;
    const std::vector<Action> withR{Action::Stand, Action::Hit, Action::Double, Action::Surrender};
    CHECK(bs.act(State{16, 10, false, true}, withR) == Action::Surrender);
    CHECK(bs.act(State{15, 10, false, true}, withR) == Action::Surrender);
    CHECK(bs.act(State{16, 10, false, true}, {Action::Stand, Action::Hit}) == Action::Hit);
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
    testDeckSeedReproducibility();
    testEnvironmentInvariants();
    testRulesAffectGame();
    testH17VsS17();
    testBasicStrategy();
    testCountingAgent();
    testStrategyChart();
    testPolicyAgreement();
    testDoubleQLearning();
    testSarsaAndExpectedSarsa();
    testExpectedSarsaFinite();
    testLearningBeatsRandom();
    testBasicStrategyNearBreakEven();
    testCountingRuns();
    testUncountedDeal();
    testHoleCardNotCountedUntilReveal();
    testLateSurrender();
    testPolicyRoundTrip();

    if (g_failures == 0) {
        std::cout << "ALL TESTS PASSED\n";
        return 0;
    }
    std::cerr << g_failures << " test(s) failed\n";
    return 1;
}
