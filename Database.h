// Database.h
// MustangMarketplace — Group 49
// Manages SQLite connection

#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <sqlite3.h>

/**
 * @file Database.h
 * @brief SQLite connection helpers for production and test modes.
 * @author Ashwin Subash (asubash2)
 */

#ifdef TESTING
/** @brief In-memory/fixture DB handle injected by acceptance tests. */
extern sqlite3* testDB;
#endif

/**
 * @class Database
 * @brief Lightweight database connection utility.
 * @author Ashwin Subash (asubash2)
 */
class Database {
public:
    /** @brief Default on-disk SQLite database path. */
    static const std::string DB_PATH;
    /**
     * @brief Opens a SQLite connection.
     * @author Ashwin Subash (asubash2)
     * @param path Optional DB file path; defaults to @ref DB_PATH.
     * @return sqlite3 handle or nullptr on failure.
     */
    static sqlite3* open(const std::string& path = DB_PATH);
    /**
     * @brief Closes a SQLite connection.
     * @author Ashwin Subash (asubash2)
     * @param db sqlite3 connection to close.
     */
    static void close(sqlite3* db);
};

#endif
