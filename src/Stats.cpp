#include "blackjack/Stats.hpp"

#include <iomanip>
#include <sstream>

namespace blackjack {

void Stats::record(double reward, bool playerBlackjack) {
    ++hands;
    bankroll += reward;
    if (reward > 0.0) {
        ++wins;
        if (playerBlackjack) ++blackjacks;
    } else if (reward < 0.0) {
        ++losses;
    } else {
        ++pushes;
    }
}

std::string Stats::summary() const {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2)
       << "hands=" << hands
       << "  win%=" << (winRate() * 100.0)
       << "  W/L/P=" << wins << "/" << losses << "/" << pushes
       << "  bankroll=" << bankroll
       << "  edge/hand=" << std::setprecision(4) << edgePerHand();
    return os.str();
}

} // namespace blackjack
