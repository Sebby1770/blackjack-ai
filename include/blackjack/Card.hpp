#pragma once

#include <string>

namespace blackjack {

enum class Suit { Hearts, Diamonds, Clubs, Spades };

// Rank is stored as 1..13 where 1 = Ace, 11 = Jack, 12 = Queen, 13 = King.
enum class Rank {
    Ace = 1, Two, Three, Four, Five, Six, Seven, Eight, Nine, Ten,
    Jack, Queen, King
};

// A single immutable playing card.
class Card {
public:
    Card(Rank rank, Suit suit);

    Rank rank() const { return rank_; }
    Suit suit() const { return suit_; }

    // Blackjack pip value. An ace returns 11; the Hand applies the soft/hard
    // (11 -> 1) reduction. Face cards (J/Q/K) return 10.
    int value() const;

    bool isAce() const { return rank_ == Rank::Ace; }

    // Pretty form using suit glyphs, e.g. "A♠", "10♥", "K♦".
    std::string toString() const;

private:
    Rank rank_;
    Suit suit_;
};

} // namespace blackjack
