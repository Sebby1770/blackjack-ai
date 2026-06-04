#include "blackjack/Card.hpp"

namespace blackjack {

Card::Card(Rank rank, Suit suit) : rank_(rank), suit_(suit) {}

int Card::value() const {
    if (rank_ == Rank::Ace) return 11;
    int r = static_cast<int>(rank_);
    return r >= 10 ? 10 : r;          // 10, J, Q, K all count as 10
}

std::string Card::toString() const {
    static const char* kRanks[] = {
        "", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"
    };
    static const char* kSuits[] = { "♥", "♦", "♣", "♠" }; // hearts diamonds clubs spades
    return std::string(kRanks[static_cast<int>(rank_)]) +
           kSuits[static_cast<int>(suit_)];
}

} // namespace blackjack
