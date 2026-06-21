#pragma once

#include <string>

#include "blackjack/Agent.hpp"

namespace blackjack {

// Render an agent's greedy policy as the canonical Blackjack strategy grid
// (rows = player total, columns = dealer up-card; S = stand, H = hit,
// D = double). For a trained agent this lets you literally see how close it
// got to textbook basic strategy.
std::string strategyChart(const Agent& agent);

} // namespace blackjack
