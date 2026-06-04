#include "blackjack/Hand.hpp"

namespace blackjack {

void Hand::add(const Card& card) { cards_.push_back(card); }
void Hand::clear() { cards_.clear(); }

std::pair<int, bool> Hand::evaluate() const {
    int total = 0;
    int aces = 0;
    for (const Card& c : cards_) {
        total += c.value();           // each ace contributes 11 here
        if (c.isAce()) ++aces;
    }
    // Demote aces from 11 to 1 while the hand would otherwise bust.
    while (total > 21 && aces > 0) {
        total -= 10;
        --aces;
    }
    bool usableAce = aces > 0;        // an ace is still counted as 11
    return {total, usableAce};
}

int  Hand::value() const  { return evaluate().first; }
bool Hand::isSoft() const { return evaluate().second; }

bool Hand::isBlackjack() const {
    return cards_.size() == 2 && value() == 21;
}

std::string Hand::toString(bool hideFirst) const {
    std::string out;
    for (std::size_t i = 0; i < cards_.size(); ++i) {
        if (i) out += " ";
        out += (hideFirst && i == 0) ? "??" : cards_[i].toString();
    }
    if (!hideFirst) {
        out += " (";
        if (isSoft()) out += "soft ";
        out += std::to_string(value());
        if (isBlackjack())  out += ", blackjack";
        else if (isBust())  out += ", bust";
        out += ")";
    }
    return out;
}

} // namespace blackjack
