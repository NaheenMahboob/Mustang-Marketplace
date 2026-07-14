/**
 * @file Admin.cpp
 * @brief Admin-only moderation operations implementation.
 *
 * Implements the administrative moderation helpers used to validate elevated
 * access and apply ban actions to user accounts.
 * @author Samuel Ross Wobschall (swobscha)
 */

#include <string>
using namespace std;
// Admin.cpp
// MustangMarketplace — Group 49

#include "Admin.h"
#include <cstdio>

const string Admin::ACCESS_CODE = "admin123";  // Shared admin access code checked before moderation actions.

bool Admin::validateCode(const string& code) {
    // Centralized comparison so server entrypoint stays clean.
    return code == ACCESS_CODE;
}

string Admin::banUser(int user_id) {
    // Verify user exists before applying moderation flag.
    sqlite3* db = Database::open();  // Database handle used for the existence check and ban update.
    if (!db) { return "DB Error"; }

    // First confirm the target user row exists so we can return a precise domain error.
    const string checkSql = "SELECT user_id FROM users WHERE user_id = ?;";  // Query that checks whether the target user exists.
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the user existence lookup.
    if (sqlite3_prepare_v2(db, checkSql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        Database::close(db);
        return "DB Error";
    }
    // Bind the requested user id into the existence check.
    sqlite3_bind_int(stmt, 1, user_id);
    // A returned row means the account exists and can be moderated.
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);  // True once the target user row is confirmed to exist.
    sqlite3_finalize(stmt);

    string result;  // Final moderation outcome returned to the caller.
    if (!found) {
        // Report missing accounts explicitly instead of pretending the update succeeded.
        result = "User Not Found";
    } else {
        // Soft moderation: preserve user row, mark as banned.
        const string updateSql = "UPDATE users SET is_banned = 1 WHERE user_id = ?;";  // Soft-ban update that flips the moderation flag.
        sqlite3_stmt* updateStmt = nullptr;  // Prepared statement for the moderation update.
        if (sqlite3_prepare_v2(db, updateSql.c_str(), -1, &updateStmt, nullptr) != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
            Database::close(db);
            return "DB Error";
        }
        // Reuse the same user id for the moderation update.
        sqlite3_bind_int(updateStmt, 1, user_id);
        // SQLITE_DONE signals that the ban flag write completed.
        result = (sqlite3_step(updateStmt) == SQLITE_DONE) ? "User banned" : "DB Error";
        sqlite3_finalize(updateStmt);
    }

    // Close the connection before returning the user-facing result.
    Database::close(db);
    // Echo the moderation result for debugging and acceptance-test output.
    printf("%s\n", result.c_str());
    return result;
}
