/**
 * @file Recommendations.cpp
 * @brief Recommendation helpers driven by the latest watchlisted listing category implementation.
 *
 * Implements the recommendation model and the service method that produces one
 * recommended listing based on recent watchlist activity.
 * @author Samuel Ross Wobschall (swobscha)
 */

#include <string>
using namespace std;
// Recommendations.cpp
// MustangMarketplace — Group 49

#include "Recommendations.h"
#include "TsvUtil.h"
#include <sstream>

// Constructor
Recommendations::Recommendations(const string& category, const string& title)
    : category(category), recommended_title(title) {}

// Getters
string Recommendations::getCategory()         const { return category; }
string Recommendations::getRecommendedTitle() const { return recommended_title; }

string Recommendations::get() {
    // API path: serialize one recommended listing row using shared TSV schema.
    if (!currentSession.active) return "DB_ERROR";

    sqlite3* db = Database::open();  // Database handle used for recommendation-history lookup and final selection.
    if (!db) return "DB_ERROR";

    // Use the latest watchlist entry as the recommendation-category signal.
    const string historySql =
        "SELECT l.category "
        "FROM watchlist w "
        "JOIN listings l ON w.listing_id = l.listing_id "
        "WHERE w.user_id=? "
        "ORDER BY w.watchlist_id DESC LIMIT 1;";  // Query that loads the latest watchlisted category for the current user.
    sqlite3_stmt* hist = nullptr;  // Prepared statement for the recommendation-history lookup.
    if (sqlite3_prepare_v2(db, historySql.c_str(), -1, &hist, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB_ERROR";
    }
    sqlite3_bind_int(hist, 1, currentSession.user_id);

    string category;  // Category signal derived from the newest watchlist entry, when one exists.
    if (sqlite3_step(hist) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(hist, 0);
        // The newest watchlist row contributes the category preference when present.
        if (text) category = reinterpret_cast<const char*>(text);
    }
    sqlite3_finalize(hist);

    // Prefer a listing matching the latest watchlisted category, excluding all watched listings.
    string sql;  // Final recommendation query chosen based on whether a category signal was found.
    if (!category.empty()) {
        // Category-driven path: match the watched category while excluding self-owned and watched listings.
        sql = "SELECT l.listing_id, l.title, l.description, l.price, l.category, l.seller_id, u.email, l.status "
              "FROM listings l JOIN users u ON l.seller_id = u.user_id "
              "WHERE l.seller_id != ? AND l.status='active' AND l.category=? "
              "AND NOT EXISTS (SELECT 1 FROM watchlist w WHERE w.user_id=? AND w.listing_id=l.listing_id) "
              "ORDER BY l.listing_id DESC LIMIT 1;";
    } else {
        // Fallback path: no watchlist category, so use the newest eligible active listing overall.
        sql = "SELECT l.listing_id, l.title, l.description, l.price, l.category, l.seller_id, u.email, l.status "
              "FROM listings l JOIN users u ON l.seller_id = u.user_id "
              "WHERE l.seller_id != ? AND l.status='active' "
              "AND NOT EXISTS (SELECT 1 FROM watchlist w WHERE w.user_id=? AND w.listing_id=l.listing_id) "
              "ORDER BY l.listing_id DESC LIMIT 1;";
    }

    sqlite3_stmt* stmt = nullptr;  // Prepared statement for selecting the final recommended listing.
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB_ERROR";
    }
    // Bind the caller id first because both SQL shapes exclude self-owned listings.
    sqlite3_bind_int(stmt, 1, currentSession.user_id);
    if (!category.empty()) {
        // The category-driven query also binds the chosen category and watchlist owner id.
        sqlite3_bind_text(stmt, 2, category.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, currentSession.user_id);
    } else {
        // The fallback query only needs the watchlist owner id for the NOT EXISTS filter.
        sqlite3_bind_int(stmt, 2, currentSession.user_id);
    }

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        // No qualifying row means the recommendation surface is currently empty.
        sqlite3_finalize(stmt);
        Database::close(db);
        return "MARKET_EMPTY";
    }

    int id = sqlite3_column_int(stmt, 0);  // Listing id of the recommended item.
    const unsigned char* title = sqlite3_column_text(stmt, 1);
    const unsigned char* desc = sqlite3_column_text(stmt, 2);
    double price = sqlite3_column_double(stmt, 3);  // Price of the recommended item.
    const unsigned char* cat = sqlite3_column_text(stmt, 4);
    int sellerId = sqlite3_column_int(stmt, 5);  // Seller id of the recommended item.
    const unsigned char* em = sqlite3_column_text(stmt, 6);
    const unsigned char* st = sqlite3_column_text(stmt, 7);

    string titleStr = title ? reinterpret_cast<const char*>(title) : "";  // Title copied out of the SQLite row.
    string descStr = desc ? reinterpret_cast<const char*>(desc) : "";  // Description copied out of the SQLite row.
    string catStr = cat ? reinterpret_cast<const char*>(cat) : "";  // Category copied out of the SQLite row.
    string email = em ? reinterpret_cast<const char*>(em) : "";  // Seller email copied out of the SQLite row.
    string status = st ? reinterpret_cast<const char*>(st) : "";  // Listing status copied out of the SQLite row.

    sqlite3_finalize(stmt);
    Database::close(db);

    ostringstream line;  // TSV serializer for the single recommended listing row.
    // Return exactly one TSV row so the existing client listing parser can reuse it unchanged.
    line << id << '\t' << tsvEscapeField(titleStr) << '\t' << tsvEscapeField(descStr) << '\t' << price << '\t'
         << tsvEscapeField(catStr) << '\t' << sellerId << '\t' << tsvEscapeField(email) << '\t'
         << tsvEscapeField(status);
    return line.str();
}
