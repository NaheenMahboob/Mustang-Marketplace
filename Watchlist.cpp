/**
 * @file Watchlist.cpp
 * @brief Watchlist model and add/remove/view operations implementation.
 *
 * Implements the watchlist entity together with the business operations used
 * to add, remove, and inspect saved listings for a user.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */

#include <string>
using namespace std;
// Watchlist.cpp
// MustangMarketplace — Group 49

#include "Watchlist.h"
#include "TsvUtil.h"
#include <cstdio>
#include <sstream>

// Constructor
Watchlist::Watchlist(int wid, int uid, int lid)
    : watchlist_id(wid), user_id(uid), listing_id(lid) {}

// Getters
int Watchlist::getWatchlistId() const { return watchlist_id; }
int Watchlist::getUserId()      const { return user_id; }
int Watchlist::getListingId() const { return listing_id; }

string Watchlist::add(int listing_id) {
    // Require authenticated user context for personalized watchlist writes.
    if (!currentSession.active) { printf("Session Expired\n"); return "Session Expired"; }

    sqlite3* db = Database::open();  // Database handle used for the add and any cleanup side-effects.
    if (!db) { return "DB Error"; }

    // UNIQUE(user_id, listing_id) drives duplicate handling branch below.
    const string insertSql = "INSERT INTO watchlist (user_id, listing_id) VALUES (?, ?);";  // Insert that creates one watchlist link for the current user.
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the watchlist insert.
    if (sqlite3_prepare_v2(db, insertSql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(stmt, 1, currentSession.user_id);
    sqlite3_bind_int(stmt, 2, listing_id);

    string result;  // Final watchlist-add outcome returned to the caller.
    int newWatchlistId = -1;  // Newly assigned watchlist row id when the insert succeeds.
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        // A successful insert becomes the user's newest watchlist row and recommendation driver.
        result = "Added to Watchlist";
        newWatchlistId = static_cast<int>(sqlite3_last_insert_rowid(db));
    } else if (sqlite3_extended_errcode(db) == SQLITE_CONSTRAINT_UNIQUE) {
        // Duplicate adds are converted into a friendly domain message by the UNIQUE constraint.
        result = "Already on Watchlist";
    } else {
        result = "DB Error";
    }

    sqlite3_finalize(stmt);
    if (result == "Added to Watchlist") {
        // If the previously recommendation-driving watchlist row points to a deleted listing,
        // unlink it now that a newer watchlist item exists.
        sqlite3_stmt* prevStmt = nullptr;  // Prepared statement for locating an older deleted driver row.
        const string prevSql =
            "SELECT w.watchlist_id "
            "FROM watchlist w "
            "JOIN listings l ON w.listing_id = l.listing_id "
            "WHERE w.user_id = ? AND w.watchlist_id < ? AND l.status = 'deleted' "
            "ORDER BY w.watchlist_id DESC LIMIT 1;";  // Query that finds the newest older deleted watchlist row.
        if (sqlite3_prepare_v2(db, prevSql.c_str(), -1, &prevStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(prevStmt, 1, currentSession.user_id);
            sqlite3_bind_int(prevStmt, 2, newWatchlistId);
            int staleWatchlistId = -1;  // Older deleted watchlist row that should now be cleaned up.
            if (sqlite3_step(prevStmt) == SQLITE_ROW) {
                // Capture the older deleted row so it can be unlinked after the new driver exists.
                staleWatchlistId = sqlite3_column_int(prevStmt, 0);
            }
            sqlite3_finalize(prevStmt);

            if (staleWatchlistId > 0) {
                sqlite3_stmt* cleanupStmt = nullptr;  // Prepared statement for deleting the stale watchlist row.
                if (sqlite3_prepare_v2(db, "DELETE FROM watchlist WHERE watchlist_id=?;", -1, &cleanupStmt, nullptr) ==
                    SQLITE_OK) {
                    sqlite3_bind_int(cleanupStmt, 1, staleWatchlistId);
                    // Remove the stale deleted row once a newer watchlist signal has replaced it.
                    if (sqlite3_step(cleanupStmt) != SQLITE_DONE) result = "DB Error";
                    sqlite3_finalize(cleanupStmt);
                } else {
                    result = "DB Error";
                }
            }
        } else {
            result = "DB Error";
        }
    }
    Database::close(db);
    printf("%s\n", result.c_str());
    return result;
}

string Watchlist::remove(int listing_id) {
    // Delete only the caller's own watchlist row for this listing.
    if (!currentSession.active) return "Session Expired";

    sqlite3* db = Database::open();  // Database handle used for the removal query.
    if (!db) return "DB Error";

    sqlite3_stmt* stmt = nullptr;  // Prepared statement for deleting the user's watchlist row.
    if (sqlite3_prepare_v2(db, "DELETE FROM watchlist WHERE user_id=? AND listing_id=?;", -1, &stmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(stmt, 1, currentSession.user_id);
    sqlite3_bind_int(stmt, 2, listing_id);
    // DELETE returning SQLITE_DONE is considered success even if the row was already absent.
    string result = (sqlite3_step(stmt) == SQLITE_DONE) ? "Removed from Watchlist" : "DB Error";  // Final watchlist-removal outcome returned to the caller.
    sqlite3_finalize(stmt);
    Database::close(db);
    return result;
}

string Watchlist::view(int user_id) {
    // Reuse listing TSV schema expected by ResponseBuilder::fromView.
    sqlite3* db = Database::open();  // Database handle used for loading the watchlist view.
    if (!db) return "DB Error";

    const string sql =
        "SELECT l.listing_id, l.title, l.description, l.price, l.category, l.seller_id, u.email, l.status "
        "FROM watchlist w "
        "JOIN listings l ON w.listing_id = l.listing_id "
        "JOIN users u ON l.seller_id = u.user_id "
        "WHERE w.user_id=? AND l.status!='deleted' "
        "ORDER BY w.watchlist_id DESC;";  // Query that returns visible watchlist listings in newest-watchlisted order.
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the watchlist-view query.
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(stmt, 1, user_id);

    ostringstream out;  // TSV serializer for the full watchlist response.
    bool any = false;  // Tracks whether at least one watchlist row was emitted.
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        // Separate serialized rows with newlines so the client can split them back apart.
        if (any) out << '\n';
        any = true;
        int id = sqlite3_column_int(stmt, 0);  // Listing id for the current watchlist row.
        string title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));  // Listing title for the current watchlist row.
        string desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));  // Listing description for the current watchlist row.
        double price = sqlite3_column_double(stmt, 3);  // Listing price for the current watchlist row.
        const unsigned char* cat = sqlite3_column_text(stmt, 4);
        string category = cat ? reinterpret_cast<const char*>(cat) : "";  // Listing category for the current watchlist row.
        int sellerId = sqlite3_column_int(stmt, 5);  // Seller id for the current watchlist row.
        const unsigned char* em = sqlite3_column_text(stmt, 6);
        string email = em ? reinterpret_cast<const char*>(em) : "";  // Seller email for the current watchlist row.
        const unsigned char* st = sqlite3_column_text(stmt, 7);
        string status = st ? reinterpret_cast<const char*>(st) : "";  // Listing status for the current watchlist row.

        ostringstream line;  // TSV serializer for this one watchlist listing row.
        // Emit the row in the same 8-column TSV schema used by other listing endpoints.
        line << id << '\t' << tsvEscapeField(title) << '\t' << tsvEscapeField(desc) << '\t' << price << '\t'
             << tsvEscapeField(category) << '\t' << sellerId << '\t' << tsvEscapeField(email) << '\t'
             << tsvEscapeField(status);
        out << line.str();
    }
    sqlite3_finalize(stmt);
    Database::close(db);
    return any ? out.str() : string();
}
