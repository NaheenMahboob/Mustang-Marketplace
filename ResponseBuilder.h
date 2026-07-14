#ifndef RESPONSE_BUILDER_H
#define RESPONSE_BUILDER_H

#include <optional>
#include <initializer_list>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "Offer.h"
#include "Notification.h"
#include "Photo.h"

/**
 * @file ResponseBuilder.h
 * @brief Converters from domain return formats into JSON API responses.
 *
 * Declares helper types and functions that normalize domain-layer return
 * values into the JSON response shapes used by the server API.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */

/**
 * @namespace ResponseBuilder
 * @brief Helpers and lightweight row types for JSON API response shaping.
 *
 * Groups the helper functions and parsed row structs that convert domain-layer
 * return values into the JSON response shapes used by the server API.
 */
namespace ResponseBuilder {

/**
 * @struct ListingRow
 * @brief Parsed listing row normalized from TSV payload.
 *
 * Stores one strongly typed listing row reconstructed from the tab-separated
 * listing payload returned by listing-oriented domain methods.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */
struct ListingRow {
    /** Persisted listing identifier parsed from the TSV row. */
    int id = -1;
    /** Listing title text returned by the domain layer. */
    std::string title;
    /** Listing description text returned by the domain layer. */
    std::string description;
    /** Listing price value parsed from the TSV row. */
    double price = 0.0;
    /** Listing category label returned by the domain layer. */
    std::string category;
    /** Seller identifier associated with the listing. */
    int sellerId = -1;
    /** Seller display name associated with the listing. */
    std::string sellerName;
    /** Listing lifecycle status label. */
    std::string status;
};

/**
 * @brief Builds a minimal success JSON response.
 *
 * Returns the standard success envelope used by endpoints that only need to
 * expose a status plus a user-facing message.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param message User-facing message.
 * @return JSON object with status "ok".
 */
nlohmann::json ok(const std::string& message);

/**
 * @brief Builds a minimal error JSON response.
 *
 * Returns the standard error envelope used when a request fails before any
 * additional payload fields need to be attached.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param message User-facing error message.
 * @return JSON object with status "error".
 */
nlohmann::json error(const std::string& message);

/**
 * @brief Builds a success response with extra fields.
 *
 * Extends the shared success envelope with endpoint-specific string fields so
 * handlers can attach additional payload values.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param message Success message.
 * @param extra Extra key-value pairs.
 * @return JSON object.
 */
nlohmann::json okWith(const std::string& message,
                      std::initializer_list<std::pair<std::string, std::string>> extra);

/**
 * @brief Parses listing TSV payload into structured rows.
 *
 * Splits the shared tab-separated listing payload format into typed
 * `ListingRow` records that later response helpers can flatten into JSON.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param payload TSV string.
 * @return Vector of ListingRow.
 */
std::vector<ListingRow> parseListingTsv(const std::string& payload);

/**
 * @brief Builds a response with flattened listing rows.
 *
 * Serializes typed listing rows into the indexed `row_*` response fields that
 * the client-side parsing logic expects.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param rows Listing rows.
 * @param message Success message.
 * @return JSON object with count and row_* fields.
 */
nlohmann::json okMapWithListings(const std::vector<ListingRow>& rows,
                                 const std::string& message);

/**
 * @brief Converts Listing::search return to JSON.
 *
 * Normalizes raw listing-search output into either an error envelope or a
 * success response containing flattened listing rows.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param listingSearchReturn Raw TSV or status.
 * @return JSON response.
 */
nlohmann::json fromSearch(const std::string& listingSearchReturn);

/**
 * @brief Converts listing view (MY_LISTINGS, etc.) to JSON.
 *
 * Reuses the shared listing-row conversion path for listing-view style
 * endpoints such as seller listings, watchlist, and purchases.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param viewReturn Raw TSV or status.
 * @return JSON response.
 */
nlohmann::json fromView(const std::string& viewReturn);

/**
 * @brief Converts recommendation result to JSON.
 *
 * Handles recommendation sentinel strings and successful TSV payloads using
 * the same flattened listing response shape as other listing endpoints.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param recommendReturn Raw TSV or sentinel.
 * @return JSON response.
 */
nlohmann::json fromRecommend(const std::string& recommendReturn);

/**
 * @brief Converts notification data to JSON.
 *
 * Flattens the notification-view result into the indexed `note_i` field shape
 * expected by the client notification parser.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param notificationViewReturn Typed notification view result.
 * @return JSON response.
 */
nlohmann::json fromNotifications(const Notification::ViewData& notificationViewReturn);

/**
 * @brief Converts incoming offers to JSON.
 *
 * Flattens seller-facing incoming-offer rows into indexed `offer_i_*` fields
 * used by the client-side offer parsing logic.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param rows Optional incoming offer rows.
 * @param seller_email Seller email.
 * @return JSON response.
 */
nlohmann::json fromIncoming(const std::optional<std::vector<IncomingOfferRow>>& rows,
                            const std::string& seller_email);

/**
 * @brief Converts outgoing offers to JSON.
 *
 * Flattens buyer-facing outgoing-offer rows into indexed `offer_i_*` fields
 * and normalizes their status labels for the client UI.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param rows Optional outgoing offer rows.
 * @return JSON response.
 */
nlohmann::json fromOutgoing(const std::optional<std::vector<OutgoingOfferRow>>& rows);

}  // namespace ResponseBuilder

#endif
