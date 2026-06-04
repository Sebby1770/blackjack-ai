#pragma once

#include <random>
#include <vector>

#include "blackjack/Card.hpp"

namespace blackjack {

// A shoe built from one or more standard 52-card decks. Dealing advances a
// cursor; shuffling rebuilds the full shoe so cards are never lost.
class Deck {
public:
    explicit Deck(int numDecks = 6, unsigned seed = std::random_device{}());

    void shuffle();                   // rebuild the full shoe and randomise it
    Card deal();                      // draw the next card (auto-reshuffles if empty)

    std::size_t remaining() const { return cards_.size() - position_; }
    std::size_t size() const { return cards_.size(); }

    // True once roughly three quarters of the shoe has been dealt -- a stand-in
    // for the casino "cut card" that triggers a reshuffle between hands.
    bool needsReshuffle() const { return remaining() < cards_.size() / 4; }

private:
    void build();                     // populate cards_ with a fresh shoe

    int numDecks_;
    std::vector<Card> cards_;
    std::size_t position_ = 0;
    std::mt19937 rng_;
};

} // namespace blackjack
