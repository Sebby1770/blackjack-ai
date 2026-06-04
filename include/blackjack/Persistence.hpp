#pragma once

#include <string>

#include "blackjack/Stats.hpp"

namespace blackjack {

// Persists evaluation results. A CSV (stats.csv) is always appended to. When
// the project is built with USE_SQLITE defined, results are additionally
// written to a SQLite database (blackjack.db) using the schema in sql/.
class StatsStore {
public:
    explicit StatsStore(const std::string& dir = ".") : dir_(dir) {}

    bool recordRun(const std::string& agent, long episodes, const Stats& s);

    // True if this build links SQLite (i.e. SQL persistence is active).
    static bool sqlEnabled();

private:
    std::string dir_;
};

} // namespace blackjack
