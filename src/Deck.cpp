#include "blackjack/Deck.hpp"

#include <algorithm>

namespace blackjack {

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
}

Card Deck::deal() {
    if (position_ >= cards_.size()) {
        shuffle();                                 // ran out -- reshuffle and continue
    }
    return cards_[position_++];
}

} // namespace blackjack
