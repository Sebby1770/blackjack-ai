#pragma once

#include <string>

#include "blackjack/Agent.hpp"
#include "blackjack/BasicStrategyAgent.hpp"

namespace blackjack {

// Render an agent's greedy policy as the canonical Blackjack strategy grid
// (rows = player total, columns = dealer up-card; S = stand, H = hit,
// D = double). For a trained agent this lets you literally see how close it
// got to textbook basic strategy.
std::string strategyChart(const Agent& agent);

// Export format for strategy grids written by exportStrategyChart().
enum class ChartFormat { Text, Markdown, Csv, Html, Json };

// Write a strategy grid in the requested format. Reuses the same state walk as
// strategyChart (hard 5-20 / soft 13-20 × dealer 2-A, canDouble = true).
std::string exportStrategyChart(const Agent& agent, ChartFormat fmt);

// Fraction of states (hard totals 4-21 × dealer 2-A × soft/hard × canDouble)
// where `learned`'s greedy action matches `basic`, considering only legal
// actions. Returns a value in [0, 1].
double policyAgreement(const Agent& learned, const BasicStrategyAgent& basic);

} // namespace blackjack
