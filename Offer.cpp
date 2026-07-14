/**
 * @file Offer.cpp
 * @brief Offer model and incoming/outgoing offer service operations implementation.
 *
 * Implements offer-related row types and the business operations used to
 * create, inspect, accept, and reject offers on listings.
 * @author Aaron Shuan Xie (axie46)
 */

#include "Offer.h"
#include "MarketplaceRules.h"
#include "User.h"
#include <cstdio>
#include <string>
using namespace std;

Offer::Offer(int oid, int lid, int bid, int sid, double price, const string& s)
    : offer_id(oid), listing_id(lid), buyer_id(bid), seller_id(sid), offer_price(price), status(s) {}

// Lightweight value accessors used by response serialization.
int Offer::getOfferId() const { return offer_id; }
int Offer::getListingId() const { return listing_id; }
int Offer::getBuyerId() const { return buyer_id; }
int Offer::getSellerId() const { return seller_id; }
double Offer::getOfferPrice() const { return offer_price; }
string Offer::getStatus() const { return status; }

string Offer::make(int listing_id, double offer_price) {
    // Validate request before touching database.
    if (!currentSession.active) return "Session Expired";
    if (offer_price <= 0) return "Invalid Offer";

    sqlite3* db = Database::open();  // Database handle used for validation, offer insertion, and side-effects.
    if (!db) return "DB Error";

    // Verify listing exists, is active, and capture seller/title for follow-up actions.
    sqlite3_stmt* listingStmt = nullptr;  // Prepared statement for loading the target listing.
    if (sqlite3_prepare_v2(db, "SELECT seller_id, status, title FROM listings WHERE listing_id=?;", -1, &listingStmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(listingStmt, 1, listing_id);

    int seller_id = -1;  // Seller id for the listing being targeted by the offer.
    string listing_status;  // Current status of the listing being targeted by the offer.
    string listing_title = "listing";  // Listing title used in seller notifications.
    bool found = false;  // Tracks whether the target listing row exists.
    if (sqlite3_step(listingStmt) == SQLITE_ROW) {
        found = true;
        seller_id = sqlite3_column_int(listingStmt, 0);
        listing_status = reinterpret_cast<const char*>(sqlite3_column_text(listingStmt, 1));
        const unsigned char* t = sqlite3_column_text(listingStmt, 2);
        if (t) listing_title = reinterpret_cast<const char*>(t);
    }
    sqlite3_finalize(listingStmt);

    if (!found || listing_status != "active") {
        // Offers can only be made against listings that still exist and are active.
        Database::close(db);
        return "Item Not Found";
    }
    if (seller_id == currentSession.user_id) {
        // Sellers cannot submit offers on their own listings.
        Database::close(db);
        return "Access Denied";
    }

    // Insert pending offer bound to current buyer and target seller.
    sqlite3_stmt* insertStmt = nullptr;  // Prepared statement for creating the pending offer row.
    const string insertSql =
        "INSERT INTO offers (listing_id, buyer_id, seller_id, offer_price, status) "
        "VALUES (?, ?, ?, ?, 'Pending');";  // Insert used to create a new pending offer.
    if (sqlite3_prepare_v2(db, insertSql.c_str(), -1, &insertStmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }

    sqlite3_bind_int(insertStmt, 1, listing_id);
    sqlite3_bind_int(insertStmt, 2, currentSession.user_id);
    sqlite3_bind_int(insertStmt, 3, seller_id);
    sqlite3_bind_double(insertStmt, 4, offer_price);

    string result = (sqlite3_step(insertStmt) == SQLITE_DONE) ? "Offer submitted" : "DB Error";  // Final make-offer outcome returned to the caller.
    sqlite3_finalize(insertStmt);

    if (result == "Offer submitted") {
        // Side-effects: ensure listing appears in buyer watchlist and notify seller.
        sqlite3_stmt* watchStmt = nullptr;  // Optional statement that ensures the listing is on the buyer's watchlist.
        if (sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO watchlist (user_id, listing_id) VALUES (?, ?);", -1, &watchStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(watchStmt, 1, currentSession.user_id);
            sqlite3_bind_int(watchStmt, 2, listing_id);
            // Keep this side-effect best-effort; duplicates are harmless here.
            sqlite3_step(watchStmt);
        }
        sqlite3_finalize(watchStmt);

        sqlite3_stmt* noteStmt = nullptr;  // Optional statement that inserts the seller notification.
        if (sqlite3_prepare_v2(db, "INSERT INTO notifications (user_id, message) VALUES (?, ?);", -1, &noteStmt, nullptr) == SQLITE_OK) {
            char offerBuf[32];
            snprintf(offerBuf, sizeof(offerBuf), "%.2f", offer_price);
            string msg = "Offer: $" + string(offerBuf) + " for " + listing_title + " from " + usernameFromEmail(currentSession.email);  // Notification text describing the new pending offer.
            sqlite3_bind_int(noteStmt, 1, seller_id);
            sqlite3_bind_text(noteStmt, 2, msg.c_str(), -1, SQLITE_TRANSIENT);
            // Notify the seller that a new pending offer was created.
            sqlite3_step(noteStmt);
        }
        sqlite3_finalize(noteStmt);
    }

    Database::close(db);
    return result;
}

optional<vector<IncomingOfferRow>> Offer::incoming(int seller_id) {
    // Seller inbox shows only pending offers on currently active listings.
    sqlite3* db = Database::open();  // Database handle used for loading incoming offers.
    if (!db) return nullopt;

    const string sql =
        "SELECT o.offer_id, o.listing_id, l.title, u.email, o.offer_price, o.status "
        "FROM offers o "
        "JOIN listings l ON o.listing_id = l.listing_id "
        "JOIN users u ON o.buyer_id = u.user_id "
        "WHERE o.seller_id = ? AND o.status = 'Pending' AND l.status = 'active' "
        "ORDER BY o.offer_id DESC;";  // Query that loads visible pending offers for a seller.
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the incoming-offers query.
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return nullopt;
    }
    sqlite3_bind_int(stmt, 1, seller_id);
    vector<IncomingOfferRow> out;  // Typed collection of incoming offers returned to the API layer.
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        IncomingOfferRow r;  // Current incoming-offer row being copied from SQLite.
        // Copy each database row into the typed struct returned to the API layer.
        r.offer_id = sqlite3_column_int(stmt, 0);
        r.listing_id = sqlite3_column_int(stmt, 1);
        r.listing_title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.buyer_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        r.offer_price = sqlite3_column_double(stmt, 4);
        r.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        out.push_back(r);
    }
    sqlite3_finalize(stmt);
    Database::close(db);
    return out;
}

optional<vector<OutgoingOfferRow>> Offer::outgoing(int buyer_id) {
    // Buyer history includes all offer statuses in reverse chronological order.
    sqlite3* db = Database::open();  // Database handle used for loading outgoing offers.
    if (!db) return nullopt;

    const string sql =
        "SELECT o.offer_id, o.listing_id, l.title, buyer.email, seller.email, o.offer_price, o.status "
        "FROM offers o "
        "JOIN listings l ON o.listing_id = l.listing_id "
        "JOIN users buyer ON o.buyer_id = buyer.user_id "
        "JOIN users seller ON o.seller_id = seller.user_id "
        "WHERE o.buyer_id = ? ORDER BY o.offer_id DESC;";  // Query that loads a buyer's full outgoing-offer history.
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the outgoing-offers query.
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return nullopt;
    }
    sqlite3_bind_int(stmt, 1, buyer_id);
    vector<OutgoingOfferRow> out;  // Typed collection of outgoing offers returned to the API layer.
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        OutgoingOfferRow r;  // Current outgoing-offer row being copied from SQLite.
        // Copy each database row into the typed struct returned to the API layer.
        r.offer_id = sqlite3_column_int(stmt, 0);
        r.listing_id = sqlite3_column_int(stmt, 1);
        r.listing_title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.buyer_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        r.seller_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        r.offer_price = sqlite3_column_double(stmt, 5);
        r.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        out.push_back(r);
    }
    sqlite3_finalize(stmt);
    Database::close(db);
    return out;
}

/**
 * @brief Shared implementation for seller accept/reject offer actions.
 *
 * Consolidates the validation, state transitions, and notification side
 * effects used by both the accept and reject offer workflows.
 * @author Aaron Shuan Xie (axie46)
 * @param offer_id Offer id being responded to.
 * @param acceptOffer True to accept, false to reject.
 * @return Operation status string.
 */
static string respondToOffer(int offer_id, bool acceptOffer) {
    // Shared accept/reject flow keeps domain checks and notification logic in one place.
    if (!currentSession.active) return "Session Expired";

    sqlite3* db = Database::open();  // Database handle used for validating and applying the offer response.
    if (!db) return "DB Error";

    const string checkSql =
        "SELECT o.buyer_id, o.seller_id, o.listing_id, o.offer_price, o.status, l.status "
        "FROM offers o JOIN listings l ON o.listing_id = l.listing_id "
        "WHERE o.offer_id = ?;";  // Query that loads all state needed to validate an accept/reject action.
    sqlite3_stmt* checkStmt = nullptr;  // Prepared statement for the offer validation lookup.
    if (sqlite3_prepare_v2(db, checkSql.c_str(), -1, &checkStmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(checkStmt, 1, offer_id);

    int buyerId = -1;  // Buyer attached to the target offer.
    int sellerId = -1;  // Seller attached to the target offer.
    int listingId = -1;  // Listing attached to the target offer.
    double offerPrice = 0.0;  // Offered price reused in the buyer notification.
    string offerStatus;  // Current status of the offer row.
    string listingStatus;  // Current status of the underlying listing.
    bool found = false;  // Tracks whether the target offer row exists.
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        found = true;
        buyerId = sqlite3_column_int(checkStmt, 0);
        sellerId = sqlite3_column_int(checkStmt, 1);
        listingId = sqlite3_column_int(checkStmt, 2);
        offerPrice = sqlite3_column_double(checkStmt, 3);
        offerStatus = reinterpret_cast<const char*>(sqlite3_column_text(checkStmt, 4));
        listingStatus = reinterpret_cast<const char*>(sqlite3_column_text(checkStmt, 5));
    }
    sqlite3_finalize(checkStmt);
    if (!found) {
        // Missing offer ids are reported as not found.
        Database::close(db);
        return "Offer Not Found";
    }
    if (sellerId != currentSession.user_id) {
        // Only the seller who received the offer may respond to it.
        Database::close(db);
        return "Access Denied";
    }
    if (offerStatus != "Pending") {
        // Only pending offers remain actionable.
        Database::close(db);
        return "Offer Not Found";
    }
    if (listingStatus != "active") {
        // The listing itself must still be active when the seller responds.
        Database::close(db);
        return "Item Not Available";
    }

    string newStatus = acceptOffer ? "Closed" : "Rejected";  // Final offer status chosen by the seller's action.
    sqlite3_stmt* updOffer = nullptr;  // Prepared statement for updating the offer row status.
    if (sqlite3_prepare_v2(db, "UPDATE offers SET status=? WHERE offer_id=?;", -1, &updOffer, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_text(updOffer, 1, newStatus.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(updOffer, 2, offer_id);
    // First persist the seller's accept/reject decision onto the offer row.
    if (sqlite3_step(updOffer) != SQLITE_DONE) {
        sqlite3_finalize(updOffer);
        Database::close(db);
        return "DB Error";
    }
    sqlite3_finalize(updOffer);

    if (acceptOffer) {
        // Accept closes sale by marking listing sold to the offer buyer.
        sqlite3_stmt* soldStmt = nullptr;  // Prepared statement that marks the listing sold when accepting.
        if (sqlite3_prepare_v2(db, "UPDATE listings SET status='sold', buyer_id=? WHERE listing_id=?;", -1, &soldStmt, nullptr) != SQLITE_OK) {
            Database::close(db);
            return "DB Error";
        }
        sqlite3_bind_int(soldStmt, 1, buyerId);
        sqlite3_bind_int(soldStmt, 2, listingId);
        // Persist the buyer id at the same time the listing becomes sold.
        if (sqlite3_step(soldStmt) != SQLITE_DONE) {
            sqlite3_finalize(soldStmt);
            Database::close(db);
            return "DB Error";
        }
        sqlite3_finalize(soldStmt);

        sqlite3_stmt* watchStmt = nullptr;  // Optional statement that ensures the buyer retains the sold listing in watchlist history.
        if (sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO watchlist (user_id, listing_id) VALUES (?, ?);", -1, &watchStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(watchStmt, 1, buyerId);
            sqlite3_bind_int(watchStmt, 2, listingId);
            // Keep the sold listing accessible to the buyer through their watchlist if needed.
            sqlite3_step(watchStmt);
        }
        sqlite3_finalize(watchStmt);
    }

    string listingTitle = "listing";  // Listing title used in the post-decision buyer notification.
    string sellerEmail = "seller";  // Seller email used to derive the display name in the buyer notification.
    sqlite3_stmt* infoStmt = nullptr;  // Prepared statement for loading listing/seller info for the notification.
    if (sqlite3_prepare_v2(db, "SELECT l.title, u.email FROM listings l JOIN users u ON l.seller_id=u.user_id WHERE l.listing_id=?;", -1, &infoStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(infoStmt, 1, listingId);
        if (sqlite3_step(infoStmt) == SQLITE_ROW) {
            const unsigned char* t = sqlite3_column_text(infoStmt, 0);
            const unsigned char* e = sqlite3_column_text(infoStmt, 1);
            if (t) listingTitle = reinterpret_cast<const char*>(t);
            if (e) sellerEmail = reinterpret_cast<const char*>(e);
        }
    }
    sqlite3_finalize(infoStmt);

    // Notify buyer about seller decision after status transition.
    sqlite3_stmt* noteStmt = nullptr;  // Prepared statement for inserting the buyer notification.
    if (sqlite3_prepare_v2(db, "INSERT INTO notifications (user_id, message) VALUES (?, ?);", -1, &noteStmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }
    char offerBuf[32];
    snprintf(offerBuf, sizeof(offerBuf), "%.2f", offerPrice);
    string msg = "Offer: $" + string(offerBuf) + " for " + listingTitle +
                 " listed by " + usernameFromEmail(sellerEmail) + " " +
                 (acceptOffer ? "accepted" : "rejected");  // Notification text describing the seller's decision.
    sqlite3_bind_int(noteStmt, 1, buyerId);
    sqlite3_bind_text(noteStmt, 2, msg.c_str(), -1, SQLITE_TRANSIENT);
    // Notify the buyer only after the offer/listing state transition is complete.
    if (sqlite3_step(noteStmt) != SQLITE_DONE) {
        sqlite3_finalize(noteStmt);
        Database::close(db);
        return "DB Error";
    }
    sqlite3_finalize(noteStmt);
    Database::close(db);
    return acceptOffer ? "Offer accepted" : "Offer rejected";
}

// Thin wrappers selecting accept/reject mode for shared handler.
string Offer::accept(int offer_id) { return respondToOffer(offer_id, true); }
string Offer::reject(int offer_id) { return respondToOffer(offer_id, false); }
