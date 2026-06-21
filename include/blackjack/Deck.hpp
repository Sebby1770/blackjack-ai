#pragma once

#include <random>
#include <vector>

#include "blackjack/Card.hpp"

namespace blackjack {

// A shoe built from one or more standard 52-card decks. Dealing advances a
// cursor; shuffling rebuilds the full shoe so cards are never lost. The shoe
// also maintains the Hi-Lo running count of everything dealt since the last
// shuffle, which the card-counting agent consumes.
class Deck {
public:
    explicit Deck(int numDecks = 6, unsigned seed = std::random_device{}());

    void shuffle();                   // rebuild the full shoe and randomise it
    Card deal();                      // draw the next card (auto-reshuffles if empty)

    std::size_t remaining() const { return cards_.size() - position_; }
    std::size_t size() const { return cards_.size(); }

    // Hi-Lo running count of the cards dealt since the last shuffle.
    int runningCount() const { return runningCount_; }

    // Decks still in the shoe (fractional) -- used to derive the true count.
    double decksRemaining() const { return static_cast<double>(remaining()) / 52.0; }

private:
    void build();                     // populate cards_ with a fresh shoe

    int numDecks_;
    std::vector<Card> cards_;
    std::size_t position_ = 0;
    int runningCount_ = 0;
    std::mt19937 rng_;
};

} // namespace blackjack
