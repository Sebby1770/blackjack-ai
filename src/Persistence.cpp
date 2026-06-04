#include "blackjack/Persistence.hpp"

#include <fstream>
#include <iostream>

#ifdef USE_SQLITE
#include <sqlite3.h>
#endif

namespace blackjack {

bool StatsStore::sqlEnabled() {
#ifdef USE_SQLITE
    return true;
#else
    return false;
#endif
}

#ifdef USE_SQLITE
namespace {
const char* kSchema =
    "CREATE TABLE IF NOT EXISTS training_runs ("
    " id            INTEGER PRIMARY KEY AUTOINCREMENT,"
    " agent         TEXT    NOT NULL,"
    " episodes      INTEGER NOT NULL,"
    " hands         INTEGER NOT NULL,"
    " wins          INTEGER NOT NULL,"
    " losses        INTEGER NOT NULL,"
    " pushes        INTEGER NOT NULL,"
    " blackjacks    INTEGER NOT NULL,"
    " bankroll      REAL    NOT NULL,"
    " win_rate      REAL    NOT NULL,"
    " edge_per_hand REAL    NOT NULL,"
    " created_at    TEXT    DEFAULT (datetime('now')));"
    "CREATE VIEW IF NOT EXISTS best_by_agent AS"
    " SELECT agent, MAX(edge_per_hand) AS best_edge_per_hand, COUNT(*) AS runs"
    " FROM training_runs GROUP BY agent;";
} // namespace
#endif

bool StatsStore::recordRun(const std::string& agent, long episodes, const Stats& s) {
    // CSV output is always written so results are never lost, regardless of
    // whether SQLite is linked.
    {
        const std::string csv = dir_ + "/stats.csv";
        const bool exists = std::ifstream(csv).good();
        std::ofstream f(csv, std::ios::app);
        if (!f) return false;
        if (!exists) {
            f << "agent,episodes,hands,wins,losses,pushes,blackjacks,"
                 "bankroll,win_rate,edge_per_hand\n";
        }
        f << agent << ',' << episodes << ',' << s.hands << ',' << s.wins << ','
          << s.losses << ',' << s.pushes << ',' << s.blackjacks << ','
          << s.bankroll << ',' << s.winRate() << ',' << s.edgePerHand() << '\n';
    }

#ifdef USE_SQLITE
    sqlite3* db = nullptr;
    const std::string path = dir_ + "/blackjack.db";
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "sqlite: open failed: " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return false;
    }

    char* err = nullptr;
    if (sqlite3_exec(db, kSchema, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "sqlite: schema error: " << (err ? err : "?") << "\n";
        sqlite3_free(err);
        sqlite3_close(db);
        return false;
    }

    const char* insert =
        "INSERT INTO training_runs"
        " (agent,episodes,hands,wins,losses,pushes,blackjacks,bankroll,"
        "  win_rate,edge_per_hand)"
        " VALUES (?,?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, insert, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "sqlite: prepare failed: " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_text  (stmt, 1, agent.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 2, episodes);
    sqlite3_bind_int64 (stmt, 3, s.hands);
    sqlite3_bind_int64 (stmt, 4, s.wins);
    sqlite3_bind_int64 (stmt, 5, s.losses);
    sqlite3_bind_int64 (stmt, 6, s.pushes);
    sqlite3_bind_int64 (stmt, 7, s.blackjacks);
    sqlite3_bind_double(stmt, 8, s.bankroll);
    sqlite3_bind_double(stmt, 9, s.winRate());
    sqlite3_bind_double(stmt, 10, s.edgePerHand());

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) std::cerr << "sqlite: insert failed: " << sqlite3_errmsg(db) << "\n";
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
#else
    return true;
#endif
}

} // namespace blackjack
