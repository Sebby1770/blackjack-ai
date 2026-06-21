#include "blackjack/Deck.hpp"

#include <algorithm>

namespace blackjack {

namespace {
// Hi-Lo tags: 2-6 -> +1, 7-9 -> 0, 10/J/Q/K/A -> -1. Card::value() returns 10
// for tens and faces and 11 for an ace, so both map to -1.
int hiLo(const Card& c) {
    const int v = c.value();
    if (v >= 2 && v <= 6) return +1;
    if (v >= 7 && v <= 9) return 0;
    return -1;
}
} // namespace

Deck::Deck(int numDecks, unsigned seed) : numDecks_(numDecks), rng_(seed) {
    build();
    shuffle();
}

void Deck::build() {
    cards_.clear();
    cards_.reserve(static_cast<std::size_t>(numDecks_) * 52);
    for (int d = 0; d < numDecks_; ++d) {
        for (int s = 0; s < 4; ++s) {
            for (int r = 1; r <= 13; ++r) {
                cards_.emplace_back(static_cast<Rank>(r), static_cast<Suit>(s));
            }
        }
    }
    position_ = 0;
}

void Deck::shuffle() {
    build();                                       // restore every card to the shoe
    std::shuffle(cards_.begin(), cards_.end(), rng_);
    position_ = 0;
    runningCount_ = 0;                             // a fresh shoe has a neutral count
}

Card Deck::deal() {
    if (position_ >= cards_.size()) {
        shuffle();                                 // ran out -- reshuffle and continue
    }
    const Card c = cards_[position_++];
    runningCount_ += hiLo(c);
    return c;
}

} // namespace blackjack
