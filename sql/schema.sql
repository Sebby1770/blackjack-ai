-- SQLite schema for Blackjack-with-AI run statistics.
--
-- This file documents the database the program creates at runtime. The same
-- DDL is embedded in src/Persistence.cpp so the binary is self-contained, but
-- you can also create/inspect the database by hand:
--
--     sqlite3 blackjack.db < sql/schema.sql
--     sqlite3 blackjack.db "SELECT * FROM training_runs ORDER BY id DESC LIMIT 10;"

CREATE TABLE IF NOT EXISTS training_runs (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    agent         TEXT    NOT NULL,           -- 'Q-Learning', 'Monte-Carlo', ...
    episodes      INTEGER NOT NULL,           -- training episodes (0 for non-learning agents)
    hands         INTEGER NOT NULL,           -- evaluation hands played
    wins          INTEGER NOT NULL,
    losses        INTEGER NOT NULL,
    pushes        INTEGER NOT NULL,
    blackjacks    INTEGER NOT NULL,
    bankroll      REAL    NOT NULL,           -- net units won/lost
    win_rate      REAL    NOT NULL,           -- wins / hands
    edge_per_hand REAL    NOT NULL,           -- bankroll / hands
    created_at    TEXT    DEFAULT (datetime('now'))
);

-- Handy view: the best result recorded for each agent.
CREATE VIEW IF NOT EXISTS best_by_agent AS
SELECT agent,
       MAX(edge_per_hand) AS best_edge_per_hand,
       COUNT(*)           AS runs
FROM training_runs
GROUP BY agent;
