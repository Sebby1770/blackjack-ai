#pragma once

#include <string>
#include <utility>
#include <vector>

#include "blackjack/Card.hpp"

namespace blackjack {

// A hand of cards with blackjack-aware scoring (soft/hard aces, bust,
// natural blackjack).
class Hand {
public:
    void add(const Card& card);
    void clear();

    // Best total <= 21 when achievable, counting each ace as 11 or 1.
    int value() const;

    // True if the hand still holds an ace counted as 11 (a "soft" hand).
    bool isSoft() const;

    bool isBust() const { return value() > 21; }

    // Natural blackjack: exactly two cards totalling 21.
    bool isBlackjack() const;

    std::size_t size() const { return cards_.size(); }
    const std::vector<Card>& cards() const { return cards_; }

    // e.g. "A♠ 7♥ (soft 18)". When `hideFirst` is set the first card is shown
    // as "??" and the total is suppressed (used for the dealer's hole card).
    std::string toString(bool hideFirst = false) const;

private:
    // Returns {best total, usableAce}.
    std::pair<int, bool> evaluate() const;

    std::vector<Card> cards_;
};

} // namespace blackjack
