/**
 * @file ResponseBuilder.cpp
 * @brief Converters from domain return formats into JSON API responses implementation.
 *
 * Implements the helper types and functions that normalize domain-layer return
 * values into the JSON response shapes used by the server API.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */

#include "ResponseBuilder.h"
#include <cstdlib>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std;

namespace ResponseBuilder {

// Build the smallest successful response envelope used by simple command-style
// actions that only need status plus a user-facing message.
json ok(const string& message) {
    // Build the shared success envelope used by endpoints with no extra data.
    return {{"status", "ok"}, {"message", message}};
}

// Build the shared failure envelope used when a request cannot complete.
json error(const string& message) {
    // Build the shared error envelope used across server handlers.
    return {{"status", "error"}, {"message", message}};
}

// Extend the shared success envelope with caller-supplied payload fields.
json okWith(const string& message, initializer_list<pair<string, string>> extra) {
    json j = {{"status", "ok"}, {"message", message}};  // Base success response being extended with extra payload fields.
    // Merge each caller-supplied field into the shared success response.
    for (const auto& kv : extra) j[kv.first] = kv.second;
    return j;
}

// Parse the domain layer's tab-separated listing payload into typed rows that
// can later be flattened into JSON.
vector<ListingRow> parseListingTsv(const string& payload) {
    vector<ListingRow> rows;  // Typed listing rows reconstructed from the raw TSV payload.
    // Empty payload means the caller has no listing rows to serialize.
    if (payload.empty()) return rows;
    istringstream in(payload);  // Stream view over the TSV payload for line-by-line parsing.
    string line;  // Current TSV row being parsed.
    while (getline(in, line)) {
        // Ignore blank lines so they do not create malformed listing rows.
        if (line.empty()) continue;
        vector<string> cols;  // Raw tab-delimited columns extracted from the current line.
        size_t start = 0;  // Start index of the next column within the current line.
        // Extract the first seven tab-delimited columns one by one.
        for (int i = 0; i < 7; i++) {
            size_t tab = line.find('\t', start);  // End position of the current column, if present.
            if (tab == string::npos) break;
            cols.push_back(line.substr(start, tab - start));
            start = tab + 1;
        }
        // The final column consumes the rest of the current line.
        cols.push_back(line.substr(start));
        // Listing TSV rows are expected to contain exactly 8 fields.
        if (cols.size() != 8) continue;
        ListingRow l;  // Strongly typed row rebuilt from the raw string columns.
        // Convert each serialized column back into its typed listing field.
        l.id = atoi(cols[0].c_str());
        l.title = cols[1];
        l.description = cols[2];
        l.price = atof(cols[3].c_str());
        l.category = cols[4];
        l.sellerId = atoi(cols[5].c_str());
        l.sellerName = cols[6];
        l.status = cols[7];
        // Append the reconstructed listing row to the parsed result set.
        rows.push_back(l);
    }
    return rows;
}

// Flatten typed listing rows into the indexed row_i_* JSON shape expected by
// the current ApiClient parsing logic.
json okMapWithListings(const vector<ListingRow>& rows, const string& message) {
    json j = {{"status", "ok"}, {"message", message}, {"count", rows.size()}};  // Success response plus the number of listing rows.
    // Flatten each typed listing row into row_i_* fields for the client.
    for (size_t i = 0; i < rows.size(); i++) {
        const auto& l = rows[i];  // Current listing row being serialized.
        string idx = to_string(i);  // Shared numeric suffix for the row_i_* fields.
        j["row_" + idx + "_id"] = l.id;
        j["row_" + idx + "_title"] = l.title;
        j["row_" + idx + "_description"] = l.description;
        j["row_" + idx + "_price"] = l.price;
        j["row_" + idx + "_category"] = l.category;
        j["row_" + idx + "_seller_id"] = l.sellerId;
        j["row_" + idx + "_seller_name"] = l.sellerName;
        j["row_" + idx + "_status"] = l.status;
    }
    return j;
}

// Convert Listing::search sentinel strings or TSV payload into JSON.
json fromSearch(const string& listingSearchReturn) {
    // Preserve database failures as explicit error responses.
    if (listingSearchReturn == "DB Error") return error("DB Error");
    // A successful search with zero matches still returns an empty listing set.
    if (listingSearchReturn == "No Results Found") return okMapWithListings({}, "OK");
    // Otherwise treat the domain-layer return as TSV listing data.
    return okMapWithListings(parseListingTsv(listingSearchReturn), "OK");
}

// Convert listing-view style payloads into the shared flattened JSON row shape.
json fromView(const string& viewReturn) {
    // Listing-view endpoints reuse the same DB-error contract as search.
    if (viewReturn == "DB Error") return error("DB Error");
    // Empty payload means the request succeeded but produced no listing rows.
    if (viewReturn.empty()) return okMapWithListings({}, "OK");
    // Non-empty payload is parsed using the shared TSV helper.
    return okMapWithListings(parseListingTsv(viewReturn), "OK");
}

// Convert recommendation payloads and sentinel values into a listing response.
json fromRecommend(const string& recommendReturn) {
    // Recommendation endpoints use distinct sentinel strings for special cases.
    if (recommendReturn == "DB_ERROR") return error("DB Error");
    // An empty market is represented as a successful response with zero rows.
    if (recommendReturn == "MARKET_EMPTY") return okMapWithListings({}, "Market Empty");
    vector<ListingRow> rows = parseListingTsv(recommendReturn);  // Parsed recommendation rows reconstructed from TSV.
    const string msg = rows.empty() ? "Market Empty" : "OK";  // User-facing message paired with the parsed rows.
    return okMapWithListings(rows, msg);
}

// Convert notification view data into the indexed note_i JSON response shape.
json fromNotifications(const Notification::ViewData& notificationViewReturn) {
    // Notification lookup failures become standard error responses.
    if (!notificationViewReturn.ok) {
        return error(notificationViewReturn.message.empty() ? "DB Error" : notificationViewReturn.message);
    }
    json j = {{"status", "ok"}, {"message", notificationViewReturn.message.empty() ? "OK" : notificationViewReturn.message}};  // Successful notification-response envelope.
    // Count tells the client how many note_i fields it should read.
    j["count"] = notificationViewReturn.notes.size();
    // Flatten the notification list into note_i fields for indexed parsing.
    for (size_t i = 0; i < notificationViewReturn.notes.size(); i++) {
        j["note_" + to_string(i)] = notificationViewReturn.notes[i];
    }
    return j;
}

// Convert incoming-offer rows into the flattened offer_i_* JSON fields used by
// the current client parser.
json fromIncoming(const optional<vector<IncomingOfferRow>>& rows, const string& seller_email) {
    // Missing optional data means the domain layer reported a failure.
    if (!rows.has_value()) return error("DB Error");
    json j = {{"status", "ok"}, {"message", "OK"}, {"count", rows->size()}};  // Successful incoming-offers response plus row count.
    // Flatten each incoming offer row into offer_i_* fields for the client.
    for (size_t i = 0; i < rows->size(); i++) {
        const auto& r = (*rows)[i];  // Current incoming-offer row being serialized.
        string idx = to_string(i);  // Shared numeric suffix for the offer_i_* fields.
        j["offer_" + idx + "_id"] = r.offer_id;
        j["offer_" + idx + "_listing_id"] = r.listing_id;
        j["offer_" + idx + "_listing_title"] = r.listing_title;
        j["offer_" + idx + "_buyer_name"] = r.buyer_name;
        j["offer_" + idx + "_owner_name"] = seller_email;
        j["offer_" + idx + "_price"] = r.offer_price;
        j["offer_" + idx + "_status"] = r.status;
    }
    return j;
}

// Convert outgoing-offer rows into flattened JSON while normalizing status
// labels for the current client UI.
json fromOutgoing(const optional<vector<OutgoingOfferRow>>& rows) {
    // Missing optional data means the domain layer reported a failure.
    if (!rows.has_value()) return error("DB Error");
    json j = {{"status", "ok"}, {"message", "OK"}, {"count", rows->size()}};  // Successful outgoing-offers response plus row count.
    // Flatten each outgoing offer row into offer_i_* fields for the client.
    for (size_t i = 0; i < rows->size(); i++) {
        const auto& r = (*rows)[i];  // Current outgoing-offer row being serialized.
        string idx = to_string(i);  // Shared numeric suffix for the offer_i_* fields.
        j["offer_" + idx + "_id"] = r.offer_id;
        j["offer_" + idx + "_listing_id"] = r.listing_id;
        j["offer_" + idx + "_listing_title"] = r.listing_title;
        j["offer_" + idx + "_buyer_name"] = r.buyer_name;
        j["offer_" + idx + "_owner_name"] = r.seller_name;
        j["offer_" + idx + "_price"] = r.offer_price;
        string status = r.status;  // Client-facing status label derived from the stored DB status.
        // Normalize database/domain wording into the UI terms used by ApiClient.
        if (status == "Closed") status = "accepted";
        else if (status == "Rejected") status = "rejected";
        else status = "pending";
        j["offer_" + idx + "_status"] = status;
    }
    return j;
}

}  // namespace ResponseBuilder
