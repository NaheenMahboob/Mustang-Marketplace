/**
 * @file server_main.cpp
 * @brief HTTP+JSON server entry point and request routing for the marketplace API.
 *
 * Owns server startup, HTTP endpoint lifecycle, token-backed session tracking,
 * and the request dispatcher that bridges JSON network actions to domain
 * operations.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Jasmine Jia Gu (jgu284)
 * @author Samuel Ross Wobschall (swobscha)
 * @author Aaron Shuan Xie (axie46)
 * @author Ashwin Subash (asubash2)
 */

#include "Admin.h"
#include "Database.h"
#include "Listing.h"
#include "Notification.h"
#include "Offer.h"
#include "Photo.h"
#include "Rating.h"
#include "Recommendations.h"
#include "ResponseBuilder.h"
#include "SessionBridge.h"
#include "User.h"
#include "Watchlist.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

using json = nlohmann::json;
using namespace httplib;
using namespace std;

/**
 * @struct SessionInfo
 * @brief Token-backed server session record.
 *
 * Stores the authenticated identity associated with one in-memory token so
 * request handlers can rehydrate user context for authenticated actions.
 */
struct SessionInfo {
    /** Authenticated user id associated with a session token. */
    int userId = -1;  // Authenticated user id associated with a session token.
    /** Authenticated email associated with a session token. */
    string email;  // Authenticated email associated with a session token.
};

/**
 * @struct MultipartActionRequest
 * @brief Represents a parsed multipart action request.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * 
 * Holds text fields plus an optional raw file (name + content).
 */
struct MultipartActionRequest {
    std::string action;  ///< Action name (e.g., "CREATE_LISTING").
    std::string token;  ///< Session token.
    std::unordered_map<std::string, std::string> fields;  ///< All text form fields.
    std::optional<std::pair<std::string, std::string>> file;  ///< Optional uploaded file as (filename, content).
};

/** In-memory session table keyed by opaque token string. */
static unordered_map<string, SessionInfo> g_sessions;  // In-memory session table keyed by opaque token string.

// Shared auth helper used by both JSON and multipart request paths.
static bool authenticateToken(const string& token, SessionInfo& session, string& error);

/**
 * @brief Bootstraps database schema from `Database.sql` at startup.
 *
 * Loads the schema script from disk and executes it as a single SQLite script
 * so a fresh server process can initialize its database automatically.
 * @author Ashwin Subash (asubash2)
 * @return True when schema script executes successfully.
 */
static bool bootstrapSchema() {
    // Load and execute schema at startup so first run is self-initializing.
    ifstream in("Database.sql");  // Input stream used to load the schema bootstrap script.
    if (!in) {
        fprintf(stderr, "Failed to open Database.sql\n");
        return false;
    }

    stringstream buffer;  // Buffer that accumulates the full schema file contents.
    buffer << in.rdbuf();
    // Execute the whole schema file as one SQLite script.
    string sql = buffer.str();  // Complete SQL bootstrap script read from Database.sql.
    if (sql.empty()) {
        fprintf(stderr, "Database.sql is empty\n");
        return false;
    }

    sqlite3* db = Database::open();  // Database handle used to execute the schema bootstrap script.
    if (!db) return false;

    char* err = nullptr;  // SQLite-owned error string populated if schema execution fails.
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);  // SQLite status code returned by the schema script execution.
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Schema bootstrap failed: %s\n", err ? err : "unknown error");
        if (err) sqlite3_free(err);
        Database::close(db);
        return false;
    }
    Database::close(db);
    return true;
}

/**
 * @brief Extracts a text parameter from a multipart form request.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param req HTTP request.
 * @param key Parameter name.
 * @param fallback Default value if missing.
 * @return Parameter value or fallback.
 */
static string getMultipartParam(const Request& req, const string& key, const string& fallback = "") {
    if (req.has_param(key)) return req.get_param_value(key);
    // cpp-httplib may expose multipart text fields through the file-part API,
    // so read their content there as a fallback for mixed text+binary forms.
    if (req.has_file(key)) return req.get_file_value(key).content;
    return fallback;
}

/**
 * @brief Parses a multipart HTTP request into a MultipartActionRequest.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param req HTTP request.
 * @return Parsed action request.
 */
static MultipartActionRequest parseMultipartAction(const Request& req) {
    MultipartActionRequest mar;  // Parsed multipart request object being populated from the HTTP request.
    mar.action = getMultipartParam(req, "action");  // Action name extracted from the multipart text fields.
    mar.token = getMultipartParam(req, "token");  // Session token extracted from the multipart text fields.

    // Copy all text parameters into the generic field map for later routing.
    for (const auto& p : req.params) {
        mar.fields[p.first] = p.second;
    }

    // Copy multipart parts as either text fields or the uploaded image payload.
    for (const auto& entry : req.files) {
        const auto& name = entry.first;  // Multipart field name for the current part.
        const auto& file = entry.second;  // Multipart part payload and metadata for the current field.
        if (name == "image") {
            mar.file = std::make_pair(file.filename, file.content);
        } else {
            mar.fields[name] = file.content;
        }
    }

    return mar;
}

/**
 * @brief Handles a multipart action request (for file uploads).
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param mar Parsed multipart action request.
 * @return JSON response.
 */
static json handleAction(const MultipartActionRequest& mar) {
    // Reuse the same token validation rules as the JSON request path.
    if (mar.action != "REGISTER" && mar.action != "LOGIN" && mar.action != "LOGOUT") {
        SessionInfo session;  // Authenticated session information loaded from the submitted token.
        string authError;  // User-facing auth error message when token validation fails.
        if (!authenticateToken(mar.token, session, authError)) {
            return ResponseBuilder::error(authError);
        }
        // Mirror the authenticated multipart request into the legacy global session bridge.
        syncSessionForNetwork(session.userId, session.email);
    }

    // Dispatch only the listing mutations that still need multipart file upload support.
    if (mar.action == "CREATE_LISTING") {
        string title = mar.fields.count("title") ? mar.fields.at("title") : "";  // Listing title submitted in the multipart form.
        string description = mar.fields.count("description") ? mar.fields.at("description") : "";  // Listing description submitted in the multipart form.
        string category = mar.fields.count("category") ? mar.fields.at("category") : "";  // Listing category submitted in the multipart form.
        double price = mar.fields.count("price") ? atof(mar.fields.at("price").c_str()) : -1.0;  // Parsed numeric listing price submitted in the multipart form.

        int newId = -1;  // Newly assigned listing id populated by Listing::create on success.
        string m = Listing::create(title, description, price, category, &newId);  // Core create-listing outcome returned by the domain layer.
        if (m != "Listing created") return ResponseBuilder::error(m);

        // Attach the uploaded raw image after the listing row is created successfully.
        if (mar.file.has_value()) {
            string saved = Photo::saveRaw(newId, mar.file->second);  // Photo-upload outcome for the raw multipart image payload.
            if (saved != "Photo saved") return ResponseBuilder::error(saved);
        }

        return ResponseBuilder::okWith(m, {{"listing_id", to_string(newId)}});
    }

    if (mar.action == "EDIT_LISTING") {
        int listingId = mar.fields.count("listing_id") ? atoi(mar.fields.at("listing_id").c_str()) : -1;  // Listing id targeted by the multipart edit request.
        string title = mar.fields.count("title") ? mar.fields.at("title") : "";  // Edited title submitted in the multipart form.
        string description = mar.fields.count("description") ? mar.fields.at("description") : "";  // Edited description submitted in the multipart form.
        double price = mar.fields.count("price") ? atof(mar.fields.at("price").c_str()) : -1.0;  // Parsed edited price submitted in the multipart form.

        string m = Listing::edit(listingId, title, description, price);  // Edit-listing outcome returned by the domain layer.
        if (m != "Listing updated") return ResponseBuilder::error(m);

        // A supplied multipart file replaces the current photo, while an empty
        // file_path field means the client explicitly requested photo removal.
        if (mar.file.has_value()) {
            string saved = Photo::saveRaw(listingId, mar.file->second);  // Photo-upload outcome for the replacement raw image payload.
            if (saved != "Photo saved") return ResponseBuilder::error(saved);
        } else if (mar.fields.count("file_path") && mar.fields.at("file_path").empty()) {
            string removed = Photo::add(listingId, "");  // Photo-removal outcome for the explicit delete-photo request.
            if (removed != "Photo removed") return ResponseBuilder::error(removed);
        }

        return ResponseBuilder::ok(m);
    }

    // You can add other actions here if you want multipart versions (e.g., for admin uploads)

    return ResponseBuilder::error("Unsupported multipart action");
}

/**
 * @brief Creates an opaque in-memory session token.
 *
 * Combines authenticated identity and runtime entropy into a token string used
 * as the lookup key in the server's in-memory session table.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param userId Authenticated user id.
 * @param email Authenticated user email.
 * @return Generated token string.
 */
static string makeToken(int userId, const string& email) {
    // Simple opaque token for in-memory session table (single-process server).
    return to_string(userId) + "_" + to_string(time(nullptr)) + "_" + to_string(rand()) + "_" + email;
}

/**
 * @brief Shared authentication helper for both JSON and multipart routes.
 *
 * Validates one opaque session token against the in-memory session table and
 * copies the associated authenticated identity on success.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param token Session token.
 * @param session Output session info.
 * @param error Output error message.
 * @return True if token is valid.
 */
static bool authenticateToken(const string& token, SessionInfo& session, string& error) {
    if (token.empty()) {
        error = "Session Expired";
        return false;
    }
    auto it = g_sessions.find(token);
    if (it == g_sessions.end()) {
        error = "Session Expired";
        return false;
    }
    session = it->second;
    return true;
}

/**
 * @brief Reads one JSON request field as text.
 *
 * Extracts a request value as a string while tolerating missing fields,
 * explicit nulls, and non-string JSON values produced by transitional client
 * payloads.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param req Parsed JSON request.
 * @param key Field name to read.
 * @param fallback Default value returned when the field is absent.
 * @return Extracted string value or fallback.
 */
static string readStringField(const json& req, const char* key, const string& fallback = "") {
    if (!req.contains(key) || req[key].is_null()) return fallback;
    if (req[key].is_string()) return req[key].get<string>();
    return req[key].dump();
}

/**
 * @brief Reads one JSON request field as an integer.
 *
 * Accepts either native numeric JSON values or string-encoded numbers so the
 * server remains compatible with the current client request formatting.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param req Parsed JSON request.
 * @param key Field name to read.
 * @param fallback Default value returned when parsing is not possible.
 * @return Parsed integer value or fallback.
 */
static int readIntField(const json& req, const char* key, int fallback) {
    if (!req.contains(key) || req[key].is_null()) return fallback;
    if (req[key].is_number_integer()) return req[key].get<int>();
    if (req[key].is_number()) return static_cast<int>(req[key].get<double>());
    if (req[key].is_string()) return atoi(req[key].get<string>().c_str());
    return fallback;
}

/**
 * @brief Reads one JSON request field as a double.
 *
 * Accepts either native numeric JSON values or string-encoded numbers so the
 * server remains compatible with the current client request formatting.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param req Parsed JSON request.
 * @param key Field name to read.
 * @param fallback Default value returned when parsing is not possible.
 * @return Parsed floating-point value or fallback.
 */
static double readDoubleField(const json& req, const char* key, double fallback) {
    if (!req.contains(key) || req[key].is_null()) return fallback;
    if (req[key].is_number()) return req[key].get<double>();
    if (req[key].is_string()) return atof(req[key].get<string>().c_str());
    return fallback;
}

/**
 * @brief Handles account registration request.
 *
 * Validates the required registration fields, delegates account creation to
 * the domain layer, and opens a new token-backed server session on success.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param req Parsed JSON request.
 * @return JSON response.
 */
static json handleRegister(const json& req) {
    string email = readStringField(req, "email", "");  // Email field submitted by the registering client.
    string password = readStringField(req, "password", "");  // Password field submitted by the registering client.
    // Registration requires both email and password fields.
    if (email.empty() || password.empty()) return ResponseBuilder::error("Invalid input");

    string msg = User::registerUser(email, password);  // Registration outcome returned by the domain layer.
    if (msg == "Registration successful") {
        int userId = currentSession.user_id;  // New user id assigned during successful registration.
        string token = makeToken(userId, email);  // Session token returned to the newly registered client.
        // Store the new session server-side before returning token details to the client.
        g_sessions[token] = SessionInfo{userId, email};
        return ResponseBuilder::okWith(msg, {{"token", token}, {"user_id", to_string(userId)}});
    }
    return ResponseBuilder::error(msg);
}

/**
 * @brief Handles login request.
 *
 * Validates the required login fields, delegates authentication to the domain
 * layer, and opens a new token-backed server session on success.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Ashwin Subash (asubash2)
 * @param req Parsed JSON request.
 * @return JSON response.
 */
static json handleLogin(const json& req) {
    string email = readStringField(req, "email", "");  // Email field submitted by the logging-in client.
    string password = readStringField(req, "password", "");  // Password field submitted by the logging-in client.
    // Login also requires both email and password fields.
    if (email.empty() || password.empty()) return ResponseBuilder::error("Invalid input");

    string msg = User::login(email, password);  // Authentication outcome returned by the domain layer.
    if (msg == "Login successful") {
        int uid = currentSession.user_id;  // Existing user id authenticated during login.
        string token = makeToken(uid, email);  // Session token returned to the logged-in client.
        // Cache the authenticated session so later requests can be authorized by token.
        g_sessions[token] = SessionInfo{uid, email};
        return ResponseBuilder::okWith(msg, {{"token", token}, {"user_id", to_string(uid)}});
    }
    return ResponseBuilder::error(msg);
}

/**
 * @brief Handles logout request by invalidating current session token.
 *
 * Removes the submitted token from the in-memory session table so future
 * authenticated requests using that token are rejected.
 * @author Jasmine Jia Gu (jgu284)
 *
 * Story 4: Close Session On Logout
 *
 * @param req Parsed JSON request.
 * @return JSON response.
 */
static json handleLogout(const json& req) {
    // Logout is best-effort: if the token is present, erase it; otherwise just
    // return a successful logged-out response so the client clears local state.
    if (req.contains("token")) {
        string token = req["token"].get<string>();  // Session token that should be invalidated.
        g_sessions.erase(token);
    }
    return ResponseBuilder::ok("Logged out");
}

/**
 * @brief Routes one parsed API request to the matching domain handler.
 *
 * Dispatches an already parsed JSON request to the correct action handler,
 * enforcing authentication for protected operations along the way.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Jasmine Jia Gu (jgu284)
 * @author Samuel Ross Wobschall (swobscha)
 * @author Aaron Shuan Xie (axie46)
 * @author Ashwin Subash (asubash2)
 * @param req Parsed JSON request.
 * @return JSON response serialized by the HTTP layer.
 */
static json handleAction(const json& req) {
    // Every request must name an action so the router knows which handler to invoke.
    if (!req.contains("action")) return ResponseBuilder::error("Missing action");
    string action = readStringField(req, "action", "");  // Action name used by the request router.

    // These three actions do not require an existing authenticated session.
    if (action == "REGISTER") return handleRegister(req);
    if (action == "LOGIN") return handleLogin(req);
    if (action == "LOGOUT") return handleLogout(req);

    // Everything below this point is authenticated API surface.
    string token = readStringField(req, "token", "");
    SessionInfo session;  // Authenticated session information loaded from the provided token.
    string authError;  // User-facing auth error message when token validation fails.
    // Stop the request at the network edge if the supplied token is missing or no longer valid.
    if (!authenticateToken(token, session, authError)) return ResponseBuilder::error(authError);

    // Mirror the token-authenticated HTTP request into the legacy global
    // session bridge before domain calls that still rely on shared session state.
    syncSessionForNetwork(session.userId, session.email);

    if (action == "SEARCH") {
        string keyword = readStringField(req, "keyword", "");  // Optional keyword substring filter from the client.
        string category = readStringField(req, "category", "All");  // Optional category filter from the client.
        string priceSort = readStringField(req, "price_sort", "none");  // Requested sort mode token for price ordering.
        // Delegate search shaping to Listing + ResponseBuilder so the router stays thin.
        return ResponseBuilder::fromSearch(Listing::search(keyword, category, priceSort));
    }

    if (action == "MY_LISTINGS") {
        return ResponseBuilder::fromView(Listing::view(session.userId));
    }

    if (action == "WATCHLIST") {
        return ResponseBuilder::fromView(Watchlist::view(session.userId));
    }

    if (action == "RECOMMEND") {
        return ResponseBuilder::fromRecommend(Recommendations::get());
    }

    if (action == "CREATE_LISTING") {
        string title = readStringField(req, "title", "");  // Listing title submitted by the client.
        string description = readStringField(req, "description", "");  // Listing description submitted by the client.
        string category = readStringField(req, "category", "");  // Listing category submitted by the client.
        double price = readDoubleField(req, "price", -1.0);  // Parsed numeric listing price submitted by the client.
        int newId = -1;  // Newly assigned listing id populated by Listing::create on success.
        string m = Listing::create(title, description, price, category, &newId);  // Core create-listing outcome returned by the domain layer.
        if (m != "Listing created") return ResponseBuilder::error(m);

        // JSON create requests now only support path-based attachment metadata.
        if (req.contains("file_path") && !req["file_path"].get<string>().empty()) {
            // File-path attachment is the fallback when no inline upload was provided.
            string pm = Photo::add(newId, readStringField(req, "file_path", ""));  // Photo-path attachment outcome for path-based uploads.
            if (pm != "Photo path set") return ResponseBuilder::error(pm);
        }

        // Return the new id so the client can navigate to/view the listing.
        return ResponseBuilder::okWith(m, {{"listing_id", to_string(newId)}});
    }

    if (action == "EDIT_LISTING") {
        int listingId = readIntField(req, "listing_id", -1);  // Listing id targeted by the mutation request.
        string title = readStringField(req, "title", "");  // Edited title submitted by the client.
        string description = readStringField(req, "description", "");  // Edited description submitted by the client.
        double price = readDoubleField(req, "price", -1.0);  // Parsed edited price submitted by the client.
        string m = Listing::edit(listingId, title, description, price);  // Edit-listing outcome returned by the domain layer.
        if (m != "Listing updated") return ResponseBuilder::error(m);

        // JSON edit requests now use only file_path presence to keep/remove/update the photo row.
        if (req.contains("file_path")) {
            // Presence of file_path, even empty, drives the keep/remove/path-update photo flow.
            string pm = Photo::add(listingId, readStringField(req, "file_path", ""));  // Photo-path or removal outcome for the edit request.
            if (pm != "Photo path set" && pm != "Photo removed") return ResponseBuilder::error(pm);
        }
        return ResponseBuilder::ok(m);
    }

    if (action == "TAG_LISTING") {
        int listingId = readIntField(req, "listing_id", -1);  // Listing id targeted by the tag request.
        string cat = readStringField(req, "category", "");  // Category submitted for the tag-listing request.
        string m = Listing::tag(listingId, cat);  // Tag-listing outcome returned by the domain layer.
        if (m == "Category assigned") return ResponseBuilder::ok(m);
        return ResponseBuilder::error(m);
    }

    if (action == "DELETE_LISTING") {
        int listingId = readIntField(req, "listing_id", -1);  // Listing id targeted by the delete request.
        string m = Listing::remove(listingId);  // Delete-listing outcome returned by the domain layer.
        if (m == "Listing removed") return ResponseBuilder::ok(m);
        return ResponseBuilder::error(m);
    }

    if (action == "ADD_WATCHLIST") {
        int listingId = readIntField(req, "listing_id", -1);  // Listing id being added to watchlist.
        string m = Watchlist::add(listingId);  // Add-watchlist outcome returned by the domain layer.
        if (m == "Added to Watchlist") return ResponseBuilder::ok(m);
        return ResponseBuilder::error(m);
    }

    if (action == "REMOVE_WATCHLIST") {
        int listingId = readIntField(req, "listing_id", -1);  // Listing id being removed from watchlist.
        string m = Watchlist::remove(listingId);  // Remove-watchlist outcome returned by the domain layer.
        if (m == "Removed from Watchlist") return ResponseBuilder::ok(m);
        return ResponseBuilder::error(m);
    }

    if (action == "MAKE_OFFER") {
        int listingId = readIntField(req, "listing_id", -1);  // Listing id the buyer is offering on.
        double offerPrice = readDoubleField(req, "offer_price", 0.0);  // Parsed offer amount submitted by the client.
        string m = Offer::make(listingId, offerPrice);  // Make-offer outcome returned by the domain layer.
        if (m == "Offer submitted") return ResponseBuilder::ok(m);
        return ResponseBuilder::error(m);
    }

    if (action == "MY_INCOMING_OFFERS") {
        return ResponseBuilder::fromIncoming(Offer::incoming(session.userId), session.email);
    }

    if (action == "ACCEPT_OFFER") {
        int offerId = readIntField(req, "offer_id", -1);  // Offer id being accepted by the seller.
        string m = Offer::accept(offerId);  // Accept-offer outcome returned by the domain layer.
        if (m == "Offer accepted") return ResponseBuilder::ok(m);
        return ResponseBuilder::error(m);
    }

    if (action == "REJECT_OFFER") {
        int offerId = readIntField(req, "offer_id", -1);  // Offer id being rejected by the seller.
        string m = Offer::reject(offerId);  // Reject-offer outcome returned by the domain layer.
        if (m == "Offer rejected") return ResponseBuilder::ok(m);
        return ResponseBuilder::error(m);
    }

    if (action == "MY_OFFERS") {
        return ResponseBuilder::fromOutgoing(Offer::outgoing(session.userId));
    }

    if (action == "NOTIFICATIONS") {
        return ResponseBuilder::fromNotifications(Notification::view(session.userId));
    }

    if (action == "BAN_USER") {
        string code = readStringField(req, "admin_code", "");  // Admin access code submitted by the client.
        int userId = readIntField(req, "user_id", -1);  // Target user id for the moderation request.
        if (!Admin::validateCode(code)) return ResponseBuilder::error("Access Denied: invalid admin code.");
        string m = Admin::banUser(userId);  // Ban-user outcome returned by the domain layer.
        if (m == "User banned") return ResponseBuilder::ok(m);
        return ResponseBuilder::error(m);
    }

    if (action == "MY_PURCHASES") {
        return ResponseBuilder::fromView(Listing::viewPurchases(session.userId));
    }

    if (action == "VIEW_RATING") {
        int sellerId = readIntField(req, "seller_id", -1);  // Seller id whose profile rating is being requested.
        // Rating view returns a formatted display string rather than raw numeric fields.
        string avg = Rating::viewSellerRating(sellerId);  // Formatted rating summary string returned by the domain layer.
        if (avg == "DB Error") return ResponseBuilder::error(avg);
        return ResponseBuilder::okWith("OK", {{"average_rating", avg}});
    }

    if (action == "CAN_RATE_SELLER") {
        int listingId = readIntField(req, "listing_id", -1);  // Purchased listing id being checked for rating eligibility.
        int sellerId = readIntField(req, "seller_id", -1);  // Seller id being checked for rating eligibility.
        string m = Rating::canRateSeller(listingId, sellerId);  // Precheck outcome describing whether the purchase can still be rated.
        if (m == "OK") return ResponseBuilder::ok(m);
        return ResponseBuilder::error(m);
    }

    if (action == "RATE_SELLER") {
        int listingId = readIntField(req, "listing_id", -1);  // Purchased listing id tied to the review submission.
        int sellerId = readIntField(req, "seller_id", -1);  // Seller id tied to the review submission.
        int rating = readIntField(req, "rating", 0);  // Parsed numeric rating value submitted by the client.
        string comment = readStringField(req, "comment", "");  // Optional free-text review comment submitted by the client.
        string m = Rating::rateSeller(listingId, sellerId, rating, comment);  // Final review-submission outcome returned by the domain layer.
        if (m == "Rating submitted") return ResponseBuilder::ok(m);
        return ResponseBuilder::error(m);
    }

    return ResponseBuilder::error("Unknown action");
}

/**
 * @brief Server entry point that initializes storage and serves HTTP requests.
 *
 * Starts the marketplace server by preparing filesystem and database state,
 * creating the HTTP server, and serving JSON API requests on `/api`.
 * @return Process exit code.
 */
int main() {
    // Seed rand() for token generation and upload filename suffixes.
    srand(static_cast<unsigned int>(time(nullptr)));
    try {
        filesystem::path dbSql = filesystem::current_path() / "Database.sql";  // Absolute path to the schema file used to infer the project root.
        if (filesystem::exists(dbSql)) {
            // Use the project directory as the base path for relative photo assets/uploads.
            Photo::setAssetRoot(filesystem::absolute(dbSql).parent_path().string());
        }
    } catch (...) {
        // Asset-root configuration is best-effort; server startup can continue without it.
    }

    if (!bootstrapSchema()) {
        fprintf(stderr, "Server startup aborted due to schema bootstrap failure.\n");
        return 1;
    }

    Server svr;  // HTTP server instance that owns the marketplace API route.

    // Expose one JSON endpoint that routes all marketplace actions by the
    // request body's `action` field rather than by many URL paths.
    svr.Post("/api", [](const Request& req, Response& res) {
        json request;  // Parsed JSON request body received from the client.
        try {
            // Parse the raw HTTP request body into a JSON object before routing.
            request = json::parse(req.body);
        } catch (const exception&) {
            // Malformed JSON is rejected before domain logic runs.
            res.status = 400;
            res.set_content(R"({"status":"error","message":"Invalid JSON"})", "application/json");
            return;
        }

        json response = handleAction(request);  // JSON response produced by the central action router.
        // Serialize the response object back to JSON and send it with the
        // correct content type so the HTTP client can parse it directly.
        res.set_content(response.dump(), "application/json");
    });

    // Expose multipart endpoints for listing create/edit so raw image uploads
    // can be sent alongside normal text fields.
    svr.Post("/create_listing", [](const Request& req, Response& res) {
        MultipartActionRequest mar = parseMultipartAction(req);  // Parsed multipart create-listing request.
        json resp = handleAction(mar);  // JSON response produced by the multipart action router.
        res.set_content(resp.dump(), "application/json");
    });

    svr.Post("/edit_listing", [](const Request& req, Response& res) {
        MultipartActionRequest mar = parseMultipartAction(req);  // Parsed multipart edit-listing request.
        json resp = handleAction(mar);  // JSON response produced by the multipart action router.
        res.set_content(resp.dump(), "application/json");
    });

    // Expose one raw photo endpoint that returns binary image bytes instead of
    // wrapping them in JSON.
    svr.Get(R"(/photo/(\d+))", [](const Request& req, Response& res) {
        int listingId = stoi(req.matches[1]);  // Listing id captured from the `/photo/<id>` route.
        string imageBytes;  // Raw image bytes loaded from disk for the requested listing photo.
        string contentType;  // MIME type inferred from the stored photo filename.
        if (!Photo::loadRaw(listingId, imageBytes, contentType)) {
            res.status = 404;
            res.set_content(R"({"status":"error","message":"Photo not found"})", "application/json");
            return;
        }
        // Send raw image bytes with the matching content type so the client can open them directly.
        res.set_content(imageBytes, contentType);
    });

    printf("Marketplace server listening on port 9090\n");
    // Listen on all interfaces so the client can connect using the configured
    // host and the shared default port 9090.
    svr.listen("0.0.0.0", 9090);

    return 0;
}
