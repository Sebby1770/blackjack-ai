#include "blackjack/Game.hpp"

#include <algorithm>
#include <iostream>
#include <string>

#include "blackjack/CountingAgent.hpp"
#include "blackjack/Environment.hpp"

namespace blackjack {

Stats evaluate(const Agent& agent, long hands, const Rules& rules, unsigned seed) {
    Environment env(rules, seed);
    Stats stats;
    for (long i = 0; i < hands; ++i) {
        Environment::Step step = env.reset();
        const bool natural = env.player().isBlackjack();
        while (!step.done) {
            const Action a = agent.act(step.state, step.legal);
            step = env.step(a);
        }
        stats.record(step.reward, natural && step.reward > 0.0);
    }
    return stats;
}

Stats evaluateCounting(const CountingAgent& agent, long hands, const Rules& rules,
                       unsigned seed) {
    Environment env(rules, seed);
    Stats stats;
    for (long i = 0; i < hands; ++i) {
        // The bet is placed before the deal, off the count carried over from
        // earlier hands in the shoe.
        const double tc  = env.trueCount();
        const double bet = agent.betUnits(tc);

        Environment::Step step = env.reset();
        const bool natural = env.player().isBlackjack();
        while (!step.done) {
            const Action a = agent.decide(step.state, step.legal, tc);
            step = env.step(a);
        }
        stats.record(step.reward * bet, natural && step.reward > 0.0);
        stats.wagered += bet;
    }
    return stats;
}

void watch(const Agent& agent, int hands, const Rules& rules, unsigned seed) {
    Environment env(rules, seed);
    for (int i = 0; i < hands; ++i) {
        std::cout << "\n--- Hand " << (i + 1) << "  (" << agent.name() << ") ---\n";
        Environment::Step s = env.reset();
        std::cout << "Dealer shows: " << env.dealerUpCard().toString() << "\n";
        std::cout << "Player:       " << env.player().toString() << "\n";
        while (!s.done) {
            const Action a = agent.act(s.state, s.legal);
            std::cout << "   -> " << toString(a) << "\n";
            s = env.step(a);
            if (!s.done) std::cout << "Player:       " << env.player().toString() << "\n";
        }
        std::cout << "Final player: " << env.player().toString() << "\n";
        std::cout << "Dealer:       " << env.dealer().toString() << "\n";
        const char* r = s.reward > 0.0 ? "WIN" : (s.reward < 0.0 ? "LOSE" : "PUSH");
        std::cout << "Result: " << r << "  (" << s.reward << ")\n";
    }
}

void playInteractive(const Agent* advisor, const Rules& rules, unsigned seed) {
    Environment env(rules, seed);
    double bankroll = 0.0;

    std::cout << "\nInteractive Blackjack -- commands: [h]it  [s]tand  [d]ouble  [q]uit\n"
                 "Dealer " << (rules.dealerHitsSoft17 ? "hits" : "stands on")
              << " soft 17. Blackjack pays " << rules.blackjackPayout << ":1.\n";

    while (true) {
        Environment::Step s = env.reset();
        std::cout << "\n=========================================\n";
        std::cout << "Dealer shows: " << env.dealerUpCard().toString() << "\n";
        std::cout << "Your hand:    " << env.player().toString() << "\n";

        if (s.done) {                                  // opening natural
            bankroll += s.reward;
            const char* msg = s.reward > 0.0 ? "Blackjack! You win"
                            : s.reward < 0.0 ? "Dealer has blackjack -- you lose"
                                             : "Both blackjack -- push";
            std::cout << msg << ".  Bankroll: " << bankroll << "\n";
        } else {
            while (!s.done) {
                if (advisor) {
                    std::cout << "(hint: " << toString(advisor->act(s.state, s.legal))
                              << ")  ";
                }
                std::cout << "Action [h/s/d/q]: ";
                std::string line;
                if (!std::getline(std::cin, line)) return;
                const char c = line.empty() ? 's' : line[0];

                if (c == 'q' || c == 'Q') {
                    std::cout << "Final bankroll: " << bankroll << "\n";
                    return;
                }

                Action a = Action::Stand;
                if (c == 'h' || c == 'H')      a = Action::Hit;
                else if (c == 'd' || c == 'D') a = Action::Double;

                if (a == Action::Double &&
                    std::find(s.legal.begin(), s.legal.end(), Action::Double) ==
                        s.legal.end()) {
                    std::cout << "Can't double now -- hitting instead.\n";
                    a = Action::Hit;
                }

                s = env.step(a);
                std::cout << "Your hand:    " << env.player().toString() << "\n";
            }
            std::cout << "Dealer hand:  " << env.dealer().toString() << "\n";
            bankroll += s.reward;
            const char* r = s.reward > 0.0 ? "You WIN"
                          : s.reward < 0.0 ? "You LOSE" : "PUSH";
            std::cout << r << "  (" << (s.reward > 0.0 ? "+" : "") << s.reward
                      << ").  Bankroll: " << bankroll << "\n";
        }

        std::cout << "Play another hand? [Y/n]: ";
        std::string line;
        if (!std::getline(std::cin, line)) return;
        if (!line.empty() && (line[0] == 'n' || line[0] == 'N')) {
            std::cout << "Final bankroll: " << bankroll << "\n";
            return;
        }
    }
}

} // namespace blackjack
