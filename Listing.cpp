/**
 * @file Listing.cpp
 * @brief Listing domain model and listing-related business operations implementation.
 *
 * Implements the marketplace listing entity together with the main create,
 * search, view, edit, and delete operations used by the application.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Jasmine Jia Gu (jgu284)
 * @author Samuel Ross Wobschall (swobscha)
 * @author Ashwin Subash (asubash2)
 */

#include <string>
using namespace std;
// Listing.cpp
// MustangMarketplace — Group 49

#include "Listing.h"
#include "Photo.h"
#include "TsvUtil.h"
#include <sstream>
#include <cstdio>

// Constructor
Listing::Listing(int lid, int sid, const string& title, const string& desc,
                 double price, const string& category, const string& status)
    : listing_id(lid), seller_id(sid), title(title), description(desc),
      price(price), category(category), status(status) {}

// Getters
int Listing::getListingId() const { return listing_id; }
int Listing::getSellerId()    const { return seller_id; }
string Listing::getTitle()       const { return title; }
string Listing::getDescription() const { return description; }
double Listing::getPrice()       const { return price; }
string Listing::getCategory()    const { return category; }
string Listing::getStatus()      const { return status; }

string Listing::create(const string& title, const string& description, double price, const string& category,
                       int* out_listing_id) {
    // Fast input guards before DB interaction.
    if (!currentSession.active) { printf("Session Expired\n"); return "Session Expired"; }
    if (title.empty() || description.empty() || price < 0) {
        printf("Invalid input\n");
        return "Invalid input";
    }

    sqlite3* db = Database::open();  // Database handle used for validation and listing creation.
    if (!db) { return "DB Error"; }

    // Enforce moderation rule: banned users cannot create new listings.
    const string checkSql = "SELECT is_banned FROM users WHERE user_id = ?;";  // Query that loads the current seller's moderation flag.
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the ban-status lookup.
    if (sqlite3_prepare_v2(db, checkSql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(stmt, 1, currentSession.user_id);

    // Read moderation state for current seller account.
    bool isBanned = false;  // Tracks whether the current seller is banned from posting listings.
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        isBanned = (sqlite3_column_int(stmt, 0) == 1);
    }
    sqlite3_finalize(stmt);

    string result;  // Final create-listing outcome returned to the caller.
    if (isBanned) {
        result = "Access Denied";
    } else {
        // Insert active listing owned by current session user.
        const string insertSql =
            "INSERT INTO listings (seller_id, title, description, price, category, status) "
            "VALUES (?, ?, ?, ?, ?, 'active');";  // Insert used to create the new active listing row.
        sqlite3_stmt* insertStmt = nullptr;  // Prepared statement for the listing insert.
        if (sqlite3_prepare_v2(db, insertSql.c_str(), -1, &insertStmt, nullptr) != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
            Database::close(db);
            return "DB Error";
        }
        sqlite3_bind_int(insertStmt, 1, currentSession.user_id);
        sqlite3_bind_text(insertStmt, 2, title.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(insertStmt, 3, description.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(insertStmt, 4, price);
        sqlite3_bind_text(insertStmt, 5, category.c_str(), -1, SQLITE_STATIC);
        // Create listing and optionally return generated listing id to caller.
        result = (sqlite3_step(insertStmt) == SQLITE_DONE) ? "Listing created" : "DB Error";
        if (result == "Listing created" && out_listing_id) {
            *out_listing_id = static_cast<int>(sqlite3_last_insert_rowid(db));
        }
        sqlite3_finalize(insertStmt);
    }

    Database::close(db);
    printf("%s\n", result.c_str());
    return result;
}

string Listing::remove(int listing_id) {
    if (!currentSession.active) { printf("Session Expired\n"); return "Session Expired"; }

    sqlite3* db = Database::open();  // Database handle used for delete validation and soft-delete updates.
    if (!db) { return "DB Error"; }

    // Validate ownership and active status before soft delete.
    const string checkSql = "SELECT seller_id, status FROM listings WHERE listing_id = ?;";  // Query that loads ownership and status for delete validation.
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the delete validation lookup.
    if (sqlite3_prepare_v2(db, checkSql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(stmt, 1, listing_id);

    // Track existence/availability/ownership for clear domain responses.
    bool found = false, isActive = false, isOwner = false;  // Flags describing existence, deletability, and ownership of the target listing.
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int seller_id = sqlite3_column_int(stmt, 0);  // Seller id loaded for the target listing.
        string status    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));  // Current listing status loaded for delete validation.
        found = true;
        isActive = (status == "active");
        isOwner = (seller_id == currentSession.user_id);
    }
    sqlite3_finalize(stmt);

    string result;  // Final delete-listing outcome returned to the caller.
    if (!found || !isActive) {
        // Sold/deleted/missing listings are all treated as unavailable for deletion.
        result = "Item Not Found";
    } else if (!isOwner) {
        // Only the seller who owns the listing may remove it.
        result = "Access Denied";
    } else {
        // Remove uploaded photo metadata/file before soft-deleting listing.
        // Keep legacy non-upload paths untouched (used by some acceptance tests).
        string existingPhotoPath;  // Stored photo path used to decide whether an upload file needs cleanup.
        sqlite3_stmt* psel = nullptr;  // Prepared statement for loading any attached photo path.
        if (sqlite3_prepare_v2(db, "SELECT file_path FROM photos WHERE listing_id = ? LIMIT 1;", -1, &psel, nullptr) ==
            SQLITE_OK) {
            sqlite3_bind_int(psel, 1, listing_id);
            if (sqlite3_step(psel) == SQLITE_ROW) {
                existingPhotoPath = reinterpret_cast<const char*>(sqlite3_column_text(psel, 0));
            }
            sqlite3_finalize(psel);
        }
        // Only managed uploads are hard-removed; legacy absolute paths are untouched.
        if (!existingPhotoPath.empty() && existingPhotoPath.rfind("uploads/", 0) == 0) {
            string photoRemove = Photo::add(listing_id, "");  // Photo-removal outcome for managed upload cleanup.
            if (photoRemove == "DB Error") {
                Database::close(db);
                return "DB Error";
            }
        }

        // Preserve listing row for soft-delete semantics.
        const string deleteSql = "UPDATE listings SET status = 'deleted' WHERE listing_id = ?;";  // Soft-delete update that hides the listing without erasing history.
        sqlite3_stmt* delStmt = nullptr;  // Prepared statement for the soft-delete update.
        if (sqlite3_prepare_v2(db, deleteSql.c_str(), -1, &delStmt, nullptr) != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
            Database::close(db);
            return "DB Error";
        }
        sqlite3_bind_int(delStmt, 1, listing_id);
        // Soft-delete by flipping the status flag instead of erasing the row entirely.
        result = (sqlite3_step(delStmt) == SQLITE_DONE) ? "Listing removed" : "DB Error";
        sqlite3_finalize(delStmt);
        if (result == "Listing removed") {
            // Immediately unlink this deleted listing from users whose watchlist has a newer driver.
            sqlite3_stmt* cleanStmt = nullptr;  // Prepared statement for post-delete watchlist cleanup.
            const string cleanupSql =
                "DELETE FROM watchlist "
                "WHERE listing_id = ? "
                "AND EXISTS ("
                "  SELECT 1 FROM watchlist newer "
                "  WHERE newer.user_id = watchlist.user_id AND newer.watchlist_id > watchlist.watchlist_id"
                ");";  // Delete that removes non-driver watchlist rows for the now-deleted listing.
            if (sqlite3_prepare_v2(db, cleanupSql.c_str(), -1, &cleanStmt, nullptr) != SQLITE_OK) {
                Database::close(db);
                return "DB Error";
            }
            sqlite3_bind_int(cleanStmt, 1, listing_id);
            // Remove only watchlist rows that are no longer the newest row for that user.
            if (sqlite3_step(cleanStmt) != SQLITE_DONE) {
                sqlite3_finalize(cleanStmt);
                Database::close(db);
                return "DB Error";
            }
            sqlite3_finalize(cleanStmt);
        }
    }

    Database::close(db);
    printf("%s\n", result.c_str());
    return result;
}

string Listing::tag(int listing_id, const string& category) {
    // Validate auth and category selection before DB work.
    if (!currentSession.active) { printf("Session Expired\n"); return "Session Expired"; }
    if (category.empty()) { printf("Selection Required\n"); return "Selection Required"; }

    // Keep category taxonomy aligned with UI/client selector options.
    const string validCategories[] = {  // Server-authoritative category taxonomy accepted by tag requests.
        "Textbook", "Furniture", "Electronics", "Clothing", "Sports", "Other"
    };
    bool valid = false;  // Tracks whether the submitted category matches one of the allowed values.
    for (const string& c : validCategories) {
        if (category == c) { valid = true; break; }
    }
    if (!valid) { printf("Selection Required\n"); return "Selection Required"; }

    sqlite3* db = Database::open();  // Database handle used for tag validation and update.
    if (!db) { return "DB Error"; }

    // Allow tagging only on active listings owned by current user.
    const string checkSql = "SELECT seller_id FROM listings WHERE listing_id = ? AND status = 'active';";  // Query that verifies the listing is active and reveals its owner.
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the tag validation lookup.
    if (sqlite3_prepare_v2(db, checkSql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(stmt, 1, listing_id);

    string result;  // Final tag-listing outcome returned to the caller.
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        // Missing or non-active listings cannot be tagged.
        result = "Item Not Found";
        sqlite3_finalize(stmt);
    } else {
        int seller_id = sqlite3_column_int(stmt, 0);  // Seller id of the listing being retagged.
        sqlite3_finalize(stmt);
        if (seller_id != currentSession.user_id) {
            // Ownership is enforced before category writes are allowed.
            result = "Access Denied";
        } else {
            // Apply category update after ownership/active checks pass.
            const string updateSql = "UPDATE listings SET category = ? WHERE listing_id = ?;";  // Update that applies the new category tag.
            sqlite3_stmt* updateStmt = nullptr;  // Prepared statement for the category update.
            if (sqlite3_prepare_v2(db, updateSql.c_str(), -1, &updateStmt, nullptr) != SQLITE_OK) {
                fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
                Database::close(db);
                return "DB Error";
            }
            sqlite3_bind_text(updateStmt, 1, category.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(updateStmt, 2, listing_id);
            result = (sqlite3_step(updateStmt) == SQLITE_DONE) ? "Category assigned" : "DB Error";
        sqlite3_finalize(updateStmt);
        }
    }

    Database::close(db);
    printf("%s\n", result.c_str());
    return result;
}

string Listing::edit(int listing_id, const string& title,
                     const string& description, double price) {
    // Reject incomplete/invalid edits early.
    if (!currentSession.active) { printf("Session Expired\n"); return "Session Expired"; }
    if (title.empty() || description.empty() || price < 0) {
        printf("Invalid input\n");
        return "Invalid input";
    }

    sqlite3* db = Database::open();  // Database handle used for edit validation and field updates.
    if (!db) { return "DB Error"; }

    // Owner check on active listing ensures edits cannot target sold/deleted items.
    const string checkSql = "SELECT seller_id FROM listings WHERE listing_id = ? AND status = 'active';";  // Query that verifies the listing is active and reveals its owner.
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the edit validation lookup.
    if (sqlite3_prepare_v2(db, checkSql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(stmt, 1, listing_id);

    string result;  // Final edit-listing outcome returned to the caller.
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        // Only existing active listings can be edited.
        result = "Item Not Found";
        sqlite3_finalize(stmt);
    } else {
        int seller_id = sqlite3_column_int(stmt, 0);  // Seller id of the listing being edited.
        sqlite3_finalize(stmt);
        if (seller_id != currentSession.user_id) {
            // Cross-account edits are rejected even if the caller knows the listing id.
            result = "Access Denied";
        } else {
            // Write new scalar fields in one update statement.
            const string updateSql =
                "UPDATE listings SET title = ?, description = ?, price = ? "
                "WHERE listing_id = ?;";  // Update that rewrites the editable listing fields.
            sqlite3_stmt* updateStmt = nullptr;  // Prepared statement for the listing-field update.
            if (sqlite3_prepare_v2(db, updateSql.c_str(), -1, &updateStmt, nullptr) != SQLITE_OK) {
                fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
                Database::close(db);
                return "DB Error";
            }
            sqlite3_bind_text(updateStmt, 1, title.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(updateStmt, 2, description.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_double(updateStmt, 3, price);
            sqlite3_bind_int(updateStmt, 4, listing_id);
            result = (sqlite3_step(updateStmt) == SQLITE_DONE) ? "Listing updated" : "DB Error";
        sqlite3_finalize(updateStmt);
        }
    }

    Database::close(db);
    printf("%s\n", result.c_str());
    return result;
}

string Listing::search(const string& keyword, const string& category, const string& price_sort) {
    sqlite3* db = Database::open();  // Database handle used for the search query.
    if (!db) return "DB Error";

    // Search active listings with optional category filter and selectable ordering.
    string sql =
        "SELECT l.listing_id, l.title, l.description, l.price, l.category, l.seller_id, u.email, l.status "
        "FROM listings l JOIN users u ON l.seller_id = u.user_id "
        "WHERE l.status='active' AND (l.title LIKE ? OR l.description LIKE ?)";  // Base search query shared by all filter combinations.
    const bool filterCategory = (!category.empty() && category != "All");  // True when the caller chose a specific category filter.
    if (filterCategory) sql += " AND l.category = ?";
    if (price_sort == "asc") sql += " ORDER BY l.price ASC, l.listing_id DESC;";
    else if (price_sort == "desc") sql += " ORDER BY l.price DESC, l.listing_id DESC;";
    else sql += " ORDER BY l.listing_id DESC;";
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the final search query.
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }
    // SQL LIKE pattern enables substring match on title/description.
    const string pattern = "%" + keyword + "%";  // SQL LIKE pattern used for substring matching.
    // Bind the same substring pattern to both title and description filters.
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_STATIC);
    // Category is bound only when the caller selected something other than All.
    if (filterCategory) sqlite3_bind_text(stmt, 3, category.c_str(), -1, SQLITE_STATIC);

    ostringstream out;  // TSV serializer for the search results.
    bool any = false;  // Tracks whether at least one search result row was emitted.
    // Serialize each row into shared 8-column TSV format.
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        // Separate serialized rows with newlines so the response builder can split them later.
        if (any) out << '\n';
        any = true;
        int id = sqlite3_column_int(stmt, 0);  // Listing id for the current search result row.
        string title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));  // Listing title for the current search result row.
        string desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));  // Listing description for the current search result row.
        double price = sqlite3_column_double(stmt, 3);  // Listing price for the current search result row.
        const unsigned char* cat = sqlite3_column_text(stmt, 4);
        string category = cat ? reinterpret_cast<const char*>(cat) : "";  // Listing category for the current search result row.
        int sellerId = sqlite3_column_int(stmt, 5);  // Seller id for the current search result row.
        const unsigned char* em = sqlite3_column_text(stmt, 6);
        string email = em ? reinterpret_cast<const char*>(em) : "";  // Seller email for the current search result row.
        const unsigned char* st = sqlite3_column_text(stmt, 7);
        string status = st ? reinterpret_cast<const char*>(st) : "";  // Listing status for the current search result row.

        ostringstream line;  // TSV serializer for this one search result row.
        line << id << '\t' << tsvEscapeField(title) << '\t' << tsvEscapeField(desc) << '\t' << price << '\t'
             << tsvEscapeField(category) << '\t' << sellerId << '\t' << tsvEscapeField(email) << '\t' << tsvEscapeField(status);
        out << line.str();
    }
    sqlite3_finalize(stmt);
    Database::close(db);
    if (!any) return "No Results Found";
    return out.str();
}

string Listing::view(int seller_id) {
    // Return seller-owned listings except soft-deleted entries.
    sqlite3* db = Database::open();  // Database handle used for the seller-listings query.
    if (!db) return "DB Error";

    const string sql =
        "SELECT l.listing_id, l.title, l.description, l.price, l.category, l.seller_id, u.email, l.status "
        "FROM listings l JOIN users u ON l.seller_id = u.user_id "
        "WHERE l.seller_id=? AND l.status!='deleted' ORDER BY l.listing_id DESC;";  // Query that loads all visible listings owned by one seller.
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the seller-listings query.
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(stmt, 1, seller_id);

    ostringstream out;  // TSV serializer for the seller-owned listings.
    bool any = false;  // Tracks whether at least one listing row was emitted.
    // Emit rows using the same schema consumed by ResponseBuilder::fromView.
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        // Separate serialized rows with newlines so the client parser sees distinct listings.
        if (any) out << '\n';
        any = true;
        int id = sqlite3_column_int(stmt, 0);  // Listing id for the current seller-owned row.
        string title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));  // Listing title for the current seller-owned row.
        string desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));  // Listing description for the current seller-owned row.
        double price = sqlite3_column_double(stmt, 3);  // Listing price for the current seller-owned row.
        const unsigned char* cat = sqlite3_column_text(stmt, 4);
        string category = cat ? reinterpret_cast<const char*>(cat) : "";  // Listing category for the current seller-owned row.
        int sid = sqlite3_column_int(stmt, 5);  // Seller id for the current seller-owned row.
        const unsigned char* em = sqlite3_column_text(stmt, 6);
        string email = em ? reinterpret_cast<const char*>(em) : "";  // Seller email for the current seller-owned row.
        const unsigned char* st = sqlite3_column_text(stmt, 7);
        string status = st ? reinterpret_cast<const char*>(st) : "";  // Listing status for the current seller-owned row.

        ostringstream line;  // TSV serializer for this one seller-owned listing row.
        line << id << '\t' << tsvEscapeField(title) << '\t' << tsvEscapeField(desc) << '\t' << price << '\t'
             << tsvEscapeField(category) << '\t' << sid << '\t' << tsvEscapeField(email) << '\t' << tsvEscapeField(status);
        out << line.str();
    }
    sqlite3_finalize(stmt);
    Database::close(db);
    // Empty string means the seller has no visible listings.
    return any ? out.str() : string();
}

string Listing::viewPurchases(int buyer_id) {
    // Show only sold listings purchased by this buyer.
    sqlite3* db = Database::open();  // Database handle used for the purchases query.
    if (!db) return "DB Error";

    const string sql =
        "SELECT l.listing_id, l.title, l.description, l.price, l.category, l.seller_id, u.email, l.status "
        "FROM listings l JOIN users u ON l.seller_id = u.user_id "
        "WHERE l.buyer_id = ? AND l.status = 'sold' ORDER BY l.listing_id DESC;";  // Query that loads sold listings purchased by one buyer.
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the purchases query.
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(stmt, 1, buyer_id);

    ostringstream out;  // TSV serializer for the purchases list.
    bool any = false;  // Tracks whether at least one purchase row was emitted.
    // Reuse listing TSV schema so client parsing stays consistent.
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        // Separate serialized rows with newlines so the client parser sees distinct purchases.
        if (any) out << '\n';
        any = true;
        int id = sqlite3_column_int(stmt, 0);  // Listing id for the current purchase row.
        string title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));  // Listing title for the current purchase row.
        string desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));  // Listing description for the current purchase row.
        double price = sqlite3_column_double(stmt, 3);  // Listing price for the current purchase row.
        const unsigned char* cat = sqlite3_column_text(stmt, 4);
        string category = cat ? reinterpret_cast<const char*>(cat) : "";  // Listing category for the current purchase row.
        int sid = sqlite3_column_int(stmt, 5);  // Seller id for the current purchase row.
        const unsigned char* em = sqlite3_column_text(stmt, 6);
        string email = em ? reinterpret_cast<const char*>(em) : "";  // Seller email for the current purchase row.
        const unsigned char* st = sqlite3_column_text(stmt, 7);
        string status = st ? reinterpret_cast<const char*>(st) : "";  // Listing status for the current purchase row.

        ostringstream line;  // TSV serializer for this one purchase row.
        line << id << '\t' << tsvEscapeField(title) << '\t' << tsvEscapeField(desc) << '\t' << price << '\t'
             << tsvEscapeField(category) << '\t' << sid << '\t' << tsvEscapeField(email) << '\t' << tsvEscapeField(status);
        out << line.str();
    }
    sqlite3_finalize(stmt);
    Database::close(db);
    // Empty string means the buyer has no completed purchases to show.
    return any ? out.str() : string();
}
