/**
 * @file ApiClient.cpp
 * @brief HTTP+JSON client for marketplace API implementation.
 *
 * Implements the client-side API surface used by the ncurses UI. All network
 * communication uses cpp-httplib and JSON.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Jasmine Jia Gu (jgu284)
 * @author Samuel Ross Wobschall (swobscha)
 * @author Aaron Shuan Xie (axie46)
 * @author Ashwin Subash (asubash2)
 */

#include "ApiClient.h"
#include "ProtocolLimits.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <cstring>
#include <fstream>

using json = nlohmann::json;
using namespace std;

// This file implements the HTTP/JSON-backed marketplace client used by the
// ncurses UI. Each public method builds a small action-oriented request,
// submits it to the server, and then translates the JSON response into the
// string map / typed row format expected by the rest of the application.

/**
 * @brief Loads a local image file into memory for multipart upload requests.
 *
 * Reads one local file from disk, validates it against the shared client-side
 * upload-size limit, and returns the raw bytes or an explanatory error.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param filePath Local filesystem path to the image file.
 * @param fileBytes Output buffer populated with raw file bytes on success.
 * @param errorMessage Output message populated when the read or validation fails.
 * @return True when the file was read successfully and passed validation.
 */
static bool readUploadFile(const string& filePath, string& fileBytes, string& errorMessage) {
    ifstream in(filePath, ios::binary | ios::ate);  // Input stream used to load the local file before multipart upload.
    if (!in) {
        errorMessage = "Cannot read image file";
        return false;
    }
    streamsize size = in.tellg();  // Local file size used for safety checks and buffer sizing.
    if (size < 0) {
        errorMessage = "Cannot read image file";
        return false;
    }
    if (static_cast<size_t>(size) > ProtocolLimits::kMaxUploadBytes) {
        errorMessage = "Image too large (max 2 MiB)";
        return false;
    }
    in.seekg(0, ios::beg);
    fileBytes.resize(static_cast<size_t>(size));
    if (!fileBytes.empty() && !in.read(&fileBytes[0], size)) {
        errorMessage = "Cannot read image file";
        fileBytes.clear();
        return false;
    }
    return true;
}

/**
 * @brief Sends one multipart listing mutation request to the server.
 *
 * Builds the multipart payload for listing create/edit flows, attaches the
 * authenticated token and optional image bytes, posts the request, and
 * normalizes the JSON response into the legacy flattened response map.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param client Configured HTTP client used to send the request.
 * @param endpoint Server endpoint path for the multipart request.
 * @param token Session token included for authenticated listing mutations.
 * @param textFields Text fields to include as multipart parts.
 * @param filePath Optional local image path to attach as raw upload bytes.
 * @param response Output map populated from the parsed JSON response body.
 * @param errorMessage Output message populated when file loading fails.
 * @return True when the request succeeds and the JSON response is parsed.
 */
static bool sendMultipartListingRequest(httplib::Client& client,
                                        const string& endpoint,
                                        const string& token,
                                        const vector<pair<string, string>>& textFields,
                                        const string* filePath,
                                        unordered_map<string, string>& response,
                                        string& errorMessage) {
    httplib::MultipartFormDataItems items;  // Multipart parts sent to the listing create/edit endpoints.
    // Copy every text field into its own multipart form part.
    for (const auto& field : textFields) {
        items.push_back({field.first, field.second, "", "text/plain"});
    }
    // Authenticated listing mutations still need the current session token.
    if (!token.empty()) {
        items.push_back({"token", token, "", "text/plain"});
    }

    string fileBytes;  // Raw file contents read from disk when an image upload is requested.
    if (filePath && !filePath->empty()) {
        // Only attach an image part when the caller explicitly supplied a local path.
        if (!readUploadFile(*filePath, fileBytes, errorMessage)) return false;
        items.push_back({"image", fileBytes, "upload.bin", "application/octet-stream"});
    }

    // POST the assembled multipart request to the create/edit listing endpoint.
    auto res = client.Post(endpoint, items);
    if (!res || res->status != 200) return false;

    try {
        json resp = json::parse(res->body);  // Parsed server response object.
        response.clear();
        // Normalize all JSON values into strings because the rest of ApiClient
        // still expects the legacy flattened string map representation.
        for (auto it = resp.begin(); it != resp.end(); ++it) {
            if (it.value().is_string())
                response[it.key()] = it.value().get<string>();
            else if (it.value().is_number())
                response[it.key()] = to_string(it.value().get<double>());
            else if (it.value().is_boolean())
                response[it.key()] = it.value().get<bool>() ? "true" : "false";
            else
                response[it.key()] = it.value().dump();
        }
        return true;
    } catch (const exception&) {
        return false;
    }
}

// Build one reusable HTTP client bound to the configured host/port so every
// later API call can reuse the same destination settings.
ApiClient::ApiClient(const string& host, int port)
    : client_(make_unique<httplib::Client>(host, port)) {}

// The default destructor is enough because the smart pointer and cached fields
// clean themselves up automatically.
ApiClient::~ApiClient() = default;

// Centralize the low-level HTTP round trip here so every higher-level API
// method can focus on its own fields and response handling instead of request
// construction, posting, and JSON parsing.
bool ApiClient::sendRequest(const string& action,
                            const vector<pair<string, string>>& fields,
                            unordered_map<string, string>& response,
                            bool needsAuth) {
    json req;  // JSON request body posted to the server.
    // Every request declares which server-side action/route should handle it.
    req["action"] = action;
    // Authenticated actions automatically carry the current session token.
    if (needsAuth && !token_.empty()) req["token"] = token_;
    // Copy endpoint-specific fields into the outgoing JSON object.
    for (const auto& kv : fields) {
        req[kv.first] = kv.second;
    }

    // POST the request to the shared marketplace API endpoint.
    auto res = client_->Post("/api", req.dump(), "application/json");
    // Network failure or non-OK HTTP status means the exchange failed before a
    // usable marketplace response could be processed.
    if (!res || res->status != 200) return false;

    try {
        json resp = json::parse(res->body);  // Parsed server response object.
        response.clear();
        // Normalize all response values into strings because the rest of the
        // client already expects a string-keyed/string-valued field map.
        for (auto it = resp.begin(); it != resp.end(); ++it) {
            if (it.value().is_string())
                response[it.key()] = it.value().get<string>();
            else if (it.value().is_number())
                response[it.key()] = to_string(it.value().get<double>());
            else if (it.value().is_boolean())
                response[it.key()] = it.value().get<bool>() ? "true" : "false";
            else
                response[it.key()] = it.value().dump();
        }
        return true;
    } catch (const exception&) {
        // Invalid/malformed JSON means the caller cannot trust the response.
        return false;
    }
}

// Convert the flattened row_i_* response representation into typed listing
// rows so the UI can consume normal structs instead of string-keyed fields.
vector<ApiListing> ApiClient::parseListings(const unordered_map<string, string>& response) {
    vector<ApiListing> out;  // Parsed listing rows returned to the caller.
    int count = 0;  // Number of rows described by the flattened response.
    auto itCount = response.find("count");  // Optional row-count field.
    if (itCount != response.end()) count = atoi(itCount->second.c_str());

    // Rebuild each row_i_* group into one strongly typed listing object.
    for (int i = 0; i < count; i++) {
        ApiListing l;  // Current listing row being reconstructed.
        string idx = to_string(i);  // Shared numeric suffix for row_i_* fields.
        auto idIt = response.find("row_" + idx + "_id");
        auto tIt = response.find("row_" + idx + "_title");
        auto dIt = response.find("row_" + idx + "_description");
        auto pIt = response.find("row_" + idx + "_price");
        auto cIt = response.find("row_" + idx + "_category");
        auto sIt = response.find("row_" + idx + "_seller_id");
        auto nIt = response.find("row_" + idx + "_seller_name");
        auto stIt = response.find("row_" + idx + "_status");
        // Only overwrite defaults for fields that are actually present.
        if (idIt != response.end()) l.id = atoi(idIt->second.c_str());
        if (tIt != response.end()) l.title = tIt->second;
        if (dIt != response.end()) l.description = dIt->second;
        if (pIt != response.end()) l.price = atof(pIt->second.c_str());
        if (cIt != response.end()) l.category = cIt->second;
        if (sIt != response.end()) l.sellerId = atoi(sIt->second.c_str());
        if (nIt != response.end()) l.sellerName = nIt->second;
        if (stIt != response.end()) l.status = stIt->second;
        // Append the fully reconstructed listing row to the output vector.
        out.push_back(l);
    }
    return out;
}

// Convert the flattened offer_i_* response representation into typed offer
// rows using the same pattern as parseListings().
vector<ApiOffer> ApiClient::parseOffers(const unordered_map<string, string>& response) {
    vector<ApiOffer> out;  // Parsed offer rows returned to the caller.
    int count = 0;  // Number of rows described by the flattened response.
    auto itCount = response.find("count");  // Optional row-count field.
    if (itCount != response.end()) count = atoi(itCount->second.c_str());
    // Rebuild each offer_i_* group into one strongly typed offer object.
    for (int i = 0; i < count; i++) {
        ApiOffer o;  // Current offer row being reconstructed.
        string idx = to_string(i);  // Shared numeric suffix for offer_i_* keys.
        auto oid = response.find("offer_" + idx + "_id");
        auto lid = response.find("offer_" + idx + "_listing_id");
        auto tIt = response.find("offer_" + idx + "_listing_title");
        auto bIt = response.find("offer_" + idx + "_buyer_name");
        auto oIt = response.find("offer_" + idx + "_owner_name");
        auto pIt = response.find("offer_" + idx + "_price");
        auto sIt = response.find("offer_" + idx + "_status");
        // Only overwrite defaults for fields that are actually present.
        if (oid != response.end()) o.offerId = atoi(oid->second.c_str());
        if (lid != response.end()) o.listingId = atoi(lid->second.c_str());
        if (tIt != response.end()) o.listingTitle = tIt->second;
        if (bIt != response.end()) o.buyerName = bIt->second;
        if (oIt != response.end()) o.ownerName = oIt->second;
        if (pIt != response.end()) o.offerPrice = atof(pIt->second.c_str());
        if (sIt != response.end()) o.status = sIt->second;
        // Append the fully reconstructed offer row to the output vector.
        out.push_back(o);
    }
    return out;
}

// ----------------------------------------------------------------------
// Public API methods – each calls sendRequest with appropriate fields.
// The response map is then used to extract status, message, and other data.
// ----------------------------------------------------------------------

// Register a new account and cache the returned session data immediately when
// the server accepts the registration.
bool ApiClient::registerUser(const string& email, const string& password, string& message) {
    unordered_map<string, string> resp;  // Parsed server response.
    if (!sendRequest("REGISTER", {{"email", email}, {"password", password}}, resp, false))
        return false;
    message = resp["message"];
    if (resp["status"] == "ok") {
        // Persist the authenticated session returned by the server.
        token_ = resp["token"];
        email_ = email;
        userId_ = stoi(resp["user_id"]);
        return true;
    }
    return false;
}

// Log in an existing user and cache the returned token/identity on success so
// later authenticated calls can attach that session automatically.
bool ApiClient::login(const string& email, const string& password, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("LOGIN", {{"email", email}, {"password", password}}, resp, false))
        return false;
    message = resp["message"];
    if (resp["status"] == "ok") {
        // Persist the authenticated session returned by the server.
        token_ = resp["token"];
        email_ = email;
        userId_ = stoi(resp["user_id"]);
        return true;
    }
    return false;
}

// Perform a best-effort server logout, then always clear the local session so
// the UI consistently exits its authenticated state.
void ApiClient::logout() {
    unordered_map<string, string> resp;
    sendRequest("LOGOUT", {}, resp, true);
    // Clear local state even if the server request fails so the UI exits the
    // authenticated mode consistently.
    token_.clear();
    email_.clear();
    userId_ = -1;
}

// Submit the search criteria, then rebuild any returned row_i_* fields into
// typed ApiListing records for the UI.
bool ApiClient::searchListings(const string& keyword, vector<ApiListing>& out, string& message,
                               const string& category, const string& priceSort) {
    unordered_map<string, string> resp;
    if (!sendRequest("SEARCH", {{"keyword", keyword}, {"category", category}, {"price_sort", priceSort}}, resp))
        return false;
    message = resp["message"];
    if (resp["status"] != "ok") return false;
    out = parseListings(resp);
    return true;
}

// Request recommendation data and return only the first parsed listing row,
// since this client API exposes a single recommended item.
bool ApiClient::getRecommendation(ApiListing& out, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("RECOMMEND", {}, resp)) return false;
    message = resp["message"];
    if (resp["status"] != "ok") return false;
    vector<ApiListing> rows = parseListings(resp);
    if (rows.empty()) return false;
    out = rows[0];
    return true;
}

// Fetch listings owned by the current user and convert the flattened response
// into typed ApiListing rows.
bool ApiClient::getMyListings(vector<ApiListing>& out, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("MY_LISTINGS", {}, resp)) return false;
    if (resp["status"] != "ok") { message = resp["message"]; return false; }
    out = parseListings(resp);
    message = resp["message"];
    return true;
}

// Fetch the authenticated user's watchlist and rebuild the returned rows into
// typed listing objects for the caller.
bool ApiClient::getWatchlist(vector<ApiListing>& out, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("WATCHLIST", {}, resp)) return false;
    if (resp["status"] != "ok") { message = resp["message"]; return false; }
    out = parseListings(resp);
    message = resp["message"];
    return true;
}

// Build a create-listing payload and use multipart upload when a local image
// path is supplied.
bool ApiClient::createListing(const string& title, const string& description, double price,
                              const string& category, string& message,
                              const string& file_path) {
    vector<pair<string, string>> fields = {
        {"action", "CREATE_LISTING"},
        {"title", title},
        {"description", description},
        {"price", to_string(price)},
        {"category", category}
    };
    unordered_map<string, string> resp;
    // Route create-listing through the multipart endpoint so raw file uploads
    // can accompany the normal text fields when needed.
    if (!sendMultipartListingRequest(*client_, "/create_listing", token_, fields,
                                     file_path.empty() ? nullptr : &file_path, resp, message)) {
        if (message.empty()) message = "Connection error";
        return false;
    }
    message = resp["message"];
    return resp["status"] == "ok";
}

// Build an edit-listing payload and optionally upload a replacement image or
// request photo removal.
bool ApiClient::editListing(int listingId, const string& title, const string& description, double price,
                            string& message, const string* file_path) {
    vector<pair<string, string>> fields = {
        {"action", "EDIT_LISTING"},
        {"listing_id", to_string(listingId)},
        {"title", title}, {"description", description}, {"price", to_string(price)}
    };
    // Presence of file_path, even empty, tells the server whether to replace,
    // remove, or leave the current photo unchanged.
    if (file_path) fields.push_back({"file_path", *file_path});
    unordered_map<string, string> resp;
    if (!sendMultipartListingRequest(*client_, "/edit_listing", token_, fields, file_path, resp, message)) {
        if (message.empty()) message = "Connection error";
        return false;
    }
    message = resp["message"];
    return resp["status"] == "ok";
}

// Send the target listing id plus its new category label to the server.
bool ApiClient::tagListing(int listingId, const string& category, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("TAG_LISTING", {{"listing_id", to_string(listingId)}, {"category", category}}, resp))
        return false;
    message = resp["message"];
    return resp["status"] == "ok";
}

// Delete the target listing and surface the server's status/message.
bool ApiClient::removeListing(int listingId, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("DELETE_LISTING", {{"listing_id", to_string(listingId)}}, resp)) return false;
    message = resp["message"];
    return resp["status"] == "ok";
}

// Add one listing to the current user's watchlist.
bool ApiClient::addToWatchlist(int listingId, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("ADD_WATCHLIST", {{"listing_id", to_string(listingId)}}, resp)) return false;
    message = resp["message"];
    return resp["status"] == "ok";
}

// Remove one listing from the current user's watchlist.
bool ApiClient::removeFromWatchlist(int listingId, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("REMOVE_WATCHLIST", {{"listing_id", to_string(listingId)}}, resp)) return false;
    message = resp["message"];
    return resp["status"] == "ok";
}

// Submit an offer for one listing using the caller-provided offered price.
bool ApiClient::makeOffer(int listingId, double offerPrice, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("MAKE_OFFER", {{"listing_id", to_string(listingId)}, {"offer_price", to_string(offerPrice)}}, resp))
        return false;
    message = resp["message"];
    return resp["status"] == "ok";
}

// Fetch offers received on the current seller's listings and rebuild the
// flattened offer_i_* response into typed offer records.
bool ApiClient::getIncomingOffers(vector<ApiOffer>& out, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("MY_INCOMING_OFFERS", {}, resp)) return false;
    if (resp["status"] != "ok") { message = resp["message"]; return false; }
    out = parseOffers(resp);
    message = resp["message"];
    return true;
}

// Fetch offers created by the current buyer and rebuild them into typed offer
// rows for the caller.
bool ApiClient::getMyOffers(vector<ApiOffer>& out, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("MY_OFFERS", {}, resp)) return false;
    if (resp["status"] != "ok") { message = resp["message"]; return false; }
    out = parseOffers(resp);
    message = resp["message"];
    return true;
}

// Accept one pending offer identified by offer id.
bool ApiClient::acceptOffer(int offerId, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("ACCEPT_OFFER", {{"offer_id", to_string(offerId)}}, resp)) return false;
    message = resp["message"];
    return resp["status"] == "ok";
}

// Reject one pending offer identified by offer id.
bool ApiClient::rejectOffer(int offerId, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("REJECT_OFFER", {{"offer_id", to_string(offerId)}}, resp)) return false;
    message = resp["message"];
    return resp["status"] == "ok";
}

// Request raw photo bytes from the dedicated image endpoint instead of the
// JSON action router.
bool ApiClient::viewPhotoRaw(int listingId, vector<uint8_t>& outRawBytes, string& message) {
    outRawBytes.clear();
    string path = "/photo/" + to_string(listingId);  // Raw photo endpoint for the selected listing.
    auto res = client_->Get(path.c_str());
    if (!res) {
        message = "Server unreachable";
        return false;
    }
    if (res->status != 200) {
        // Error responses still come back as JSON, so parse them when possible.
        try {
            json err = json::parse(res->body);
            message = err.value("message", "Photo not found");
        } catch (...) {
            message = "Photo not found";
        }
        return false;
    }
    // Success responses carry raw image bytes directly in the body.
    outRawBytes.assign(res->body.begin(), res->body.end());
    message = "Image retrieved";
    return true;
}

// Fetch notifications for the current user and unpack the flattened note_i
// fields into a simple vector of strings.
bool ApiClient::getNotifications(vector<string>& out, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("NOTIFICATIONS", {}, resp)) return false;
    if (resp["status"] != "ok") { message = resp["message"]; return false; }
    out.clear();
    int count = atoi(resp["count"].c_str());  // Number of flattened note_i fields.
    for (int i = 0; i < count; i++) {
        string key = "note_" + to_string(i);
        if (resp.count(key)) out.push_back(resp[key]);
    }
    message = resp["message"];
    return true;
}

// Submit an admin ban request for the target user id.
bool ApiClient::banUser(const string& adminCode, int userId, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("BAN_USER", {{"admin_code", adminCode}, {"user_id", to_string(userId)}}, resp))
        return false;
    message = resp["message"];
    return resp["status"] == "ok";
}

// Fetch the current user's purchase history and rebuild the returned listing
// rows into typed ApiListing objects.
bool ApiClient::getMyPurchases(vector<ApiListing>& out, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("MY_PURCHASES", {}, resp)) { message = "Connection error"; return false; }
    if (resp["status"] != "ok") { message = resp["message"]; return false; }
    out = parseListings(resp);
    message = resp["message"];
    return true;
}

// Fetch the seller's rating summary and extract the optional average_rating
// field when the request succeeds.
bool ApiClient::getSellerRating(int sellerId, string& averageRating, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("VIEW_RATING", {{"seller_id", to_string(sellerId)}}, resp)) {
        message = "Connection error";
        return false;
    }
    message = resp["message"];
    if (resp["status"] != "ok") return false;
    averageRating = resp.count("average_rating") ? resp["average_rating"] : "";
    return true;
}

// Ask the server whether the current user is allowed to rate the specified
// seller for the specified listing.
bool ApiClient::canRateSeller(int listingId, int sellerId, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("CAN_RATE_SELLER", {{"listing_id", to_string(listingId)}, {"seller_id", to_string(sellerId)}}, resp)) {
        message = "Connection error";
        return false;
    }
    message = resp["message"];
    return resp["status"] == "ok";
}

// Submit a seller rating, optionally with a free-form review comment, for one
// purchased listing.
bool ApiClient::rateSeller(int listingId, int sellerId, int rating, const string& comment, string& message) {
    unordered_map<string, string> resp;
    if (!sendRequest("RATE_SELLER", {{"listing_id", to_string(listingId)}, {"seller_id", to_string(sellerId)},
                                     {"rating", to_string(rating)}, {"comment", comment}}, resp)) {
        message = "Connection error";
        return false;
    }
    message = resp["message"];
    return resp["status"] == "ok";
}

// A non-empty token means the client currently considers itself logged in.
bool ApiClient::isLoggedIn() const { return !token_.empty(); }

// Return the locally cached authenticated email.
const string& ApiClient::getCurrentEmail() const { return email_; }

// Return the locally cached authenticated user id, or -1 when logged out.
int ApiClient::getCurrentUserId() const { return userId_; }
