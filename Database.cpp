/**
 * @file Database.cpp
 * @brief SQLite connection helpers for production and test modes implementation.
 * @author Ashwin Subash (asubash2)
 */

#include <string>
using namespace std;
// Database.cpp
// MustangMarketplace — Group 49

#include "Database.h"
#include <cstdio>

const string Database::DB_PATH = "marketplace.db";  // Default on-disk SQLite database path for production runs.

sqlite3* Database::open(const string& path) {
#ifdef TESTING
    // In tests, reuse injected in-memory handle for deterministic isolation.
    return testDB;
#endif
    // Production path: open on-disk DB and enable FK enforcement.
    sqlite3* db = nullptr;  // SQLite connection handle opened for this request.
    // Ask SQLite to open the configured database file into this handle.
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        fprintf(stderr, "Failed to open DB: %s\n", sqlite3_errmsg(db));
        return nullptr;
    }
    // Enable foreign-key enforcement on every real connection before queries run.
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    return db;
}

void Database::close(sqlite3* db) {
#ifndef TESTING
    // In production, close each opened sqlite handle.
    sqlite3_close(db);
#endif
    // Under TESTING the shared in-memory DB stays open for the whole suite.
}
