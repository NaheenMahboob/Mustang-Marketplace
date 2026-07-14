/**
 * @file Rating.cpp
 * @brief Purchase-backed seller rating model and buyer-authenticated rating operation implementation.
 *
 * Implements the seller-rating entity and the purchase-aware service
 * operations used to view and submit seller ratings.
 * @author Aaron Shuan Xie (axie46)
 * @author Ashwin Subash (asubash2)
 */

#include "Rating.h"
#include <cstdio>
#include <sstream>
#include <iomanip>
using namespace std;

Rating::Rating(int rid, int lid, int bid, int sid, int val, const string& comment)
    : rating_id(rid), listing_id(lid), buyer_id(bid), seller_id(sid), rating_value(val), comment(comment) {}

// Lightweight value accessors for potential UI/reporting use.
int Rating::getRatingId() const { return rating_id; }
int Rating::getListingId() const { return listing_id; }
int Rating::getBuyerId() const { return buyer_id; }
int Rating::getSellerId() const { return seller_id; }
int Rating::getRatingValue() const { return rating_value; }
string Rating::getComment() const { return comment; }

string Rating::viewSellerRating(int seller_id) {
    // Invalid seller ids behave like "no data" instead of hitting the database meaninglessly.
    if (seller_id <= 0) return "No ratings yet";

    sqlite3* db = Database::open();  // Database handle used for seller lookup and rating aggregation.
    if (!db) return "DB Error";

    // Hide seller profile summary when the account is currently banned.
    const string sellerSql = "SELECT is_banned FROM users WHERE user_id = ?;";  // Query that loads the seller's current moderation flag.
    sqlite3_stmt* sellerStmt = nullptr;  // Prepared statement for the seller moderation lookup.
    if (sqlite3_prepare_v2(db, sellerSql.c_str(), -1, &sellerStmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(sellerStmt, 1, seller_id);
    if (sqlite3_step(sellerStmt) != SQLITE_ROW) {
        // Unknown sellers are treated the same as unrated sellers in the view layer.
        sqlite3_finalize(sellerStmt);
        Database::close(db);
        return "No ratings yet";
    }
    if (sqlite3_column_int(sellerStmt, 0) != 0) {
        // Moderation hides the rating summary regardless of stored review history.
        sqlite3_finalize(sellerStmt);
        Database::close(db);
        return "Profile Hidden";
    }
    sqlite3_finalize(sellerStmt);

    const string avgSql =
        "SELECT AVG(rating), COUNT(*) FROM ratings WHERE seller_id = ?;";  // Aggregate query that computes the seller average and total review count.

    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the average/count query.
    if (sqlite3_prepare_v2(db, avgSql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(stmt, 1, seller_id);

    string result = "No ratings yet";  // Final rating-summary string returned to the caller.
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
            // Format the average exactly as "<avg to 1 decimal> (<count>)" for direct UI display.
            double avg = sqlite3_column_double(stmt, 0);  // Average numeric rating for the seller.
            int count = sqlite3_column_int(stmt, 1);  // Number of ratings contributing to that average.
            ostringstream out;  // Formatter used to build the final display string.
            out << fixed << setprecision(1) << avg << " (" << count << ")";
            result = out.str();
        }
    } else {
        // Failing to step the aggregate query means the DB read failed.
        result = "DB Error";
    }

    sqlite3_finalize(stmt);
    Database::close(db);
    return result;
}

string Rating::canRateSeller(int listing_id, int seller_id) {
    if (!currentSession.active) return "Session Expired";
    if (listing_id <= 0 || seller_id <= 0) return "No completed transaction";

    sqlite3* db = Database::open();  // Database handle used for purchase and duplicate-review checks.
    if (!db) return "DB Error";

    // Gate rating to the exact sold listing purchased by the current buyer.
    const string checkSql =
        "SELECT listing_id FROM listings "
        "WHERE listing_id = ? AND seller_id = ? AND buyer_id = ? AND status = 'sold' LIMIT 1;";  // Query that proves this sold listing belongs to the current buyer.

    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the completed-purchase eligibility check.
    sqlite3_prepare_v2(db, checkSql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, listing_id);
    sqlite3_bind_int(stmt, 2, seller_id);
    sqlite3_bind_int(stmt, 3, currentSession.user_id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        // Ratings are limited to sold listings actually purchased by this buyer from this seller.
        sqlite3_finalize(stmt);
        Database::close(db);
        return "No completed transaction";
    }
    sqlite3_finalize(stmt);

    // Enforce one rating per purchased listing.
    const string existingRatingSql =
        "SELECT rating_id FROM ratings "
        "WHERE buyer_id = ? AND listing_id = ? LIMIT 1;";  // Query that detects an existing rating for this purchase.

    sqlite3_stmt* existingStmt = nullptr;  // Prepared statement for the duplicate-rating lookup.
    sqlite3_prepare_v2(db, existingRatingSql.c_str(), -1, &existingStmt, nullptr);
    sqlite3_bind_int(existingStmt, 1, currentSession.user_id);
    sqlite3_bind_int(existingStmt, 2, listing_id);

    if (sqlite3_step(existingStmt) == SQLITE_ROW) {
        // Only one review row is allowed per purchased listing.
        sqlite3_finalize(existingStmt);
        Database::close(db);
        return "Already rated this purchase";
    }
    sqlite3_finalize(existingStmt);
    Database::close(db);
    return "OK";
}

string Rating::rateSeller(int listing_id, int seller_id, int rating, const string& comment) {
    // Validate authenticated context and rating bounds first.
    if (!currentSession.active) return "Session Expired";
    if (rating < 1 || rating > 5) return "Invalid rating";
    string eligibility = canRateSeller(listing_id, seller_id);  // Eligibility result shared with the UI precheck flow.
    // Reuse the same eligibility gate the UI precheck uses so the rules stay consistent.
    if (eligibility != "OK") return eligibility;

    // Persist final rating row once all guards pass.
    sqlite3* db = Database::open();  // Database handle used for inserting the new review row.
    if (!db) return "DB Error";
    const string insertSql =
        "INSERT INTO ratings (listing_id, buyer_id, seller_id, rating, comment) VALUES (?, ?, ?, ?, ?);";  // Insert used to persist one review row.

    sqlite3_stmt* insertStmt = nullptr;  // Prepared statement for the review insert.
    sqlite3_prepare_v2(db, insertSql.c_str(), -1, &insertStmt, nullptr);
    sqlite3_bind_int(insertStmt, 1, listing_id);
    sqlite3_bind_int(insertStmt, 2, currentSession.user_id);
    sqlite3_bind_int(insertStmt, 3, seller_id);
    sqlite3_bind_int(insertStmt, 4, rating);
    sqlite3_bind_text(insertStmt, 5, comment.c_str(), -1, SQLITE_STATIC);

    // Insert the final review row tied to the specific completed purchase.
    string result = (sqlite3_step(insertStmt) == SQLITE_DONE)  // Final rating-submission outcome returned to the caller.
                        ? "Rating submitted"
                        : "DB Error";

    sqlite3_finalize(insertStmt);
    Database::close(db);

    printf("%s\n", result.c_str());
    return result;
}
