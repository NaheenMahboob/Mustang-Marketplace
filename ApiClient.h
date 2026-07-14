#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <httplib.h>

/**
 * @file ApiClient.h
 * @brief HTTP+JSON client for marketplace API.
 *
 * Declares the client-side API surface used by the ncurses UI.
 * All network communication uses cpp-httplib and JSON.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Jasmine Jia Gu (jgu284)
 * @author Samuel Ross Wobschall (swobscha)
 * @author Aaron Shuan Xie (axie46)
 * @author Ashwin Subash (asubash2)
 */

/**
 * @struct ApiListing
 * @brief Parsed listing row returned to the client UI.
 * @author Muhammad Naheen Mahboob (mmahbo)
 *
 * Stores one normalized listing row reconstructed from API response fields
 * returned by the server.
 */
struct ApiListing {
    /** Persisted listing id returned by the server. */
    int id = -1;
    /** Seller id associated with the listing row. */
    int sellerId = -1;
    /** Seller display name reconstructed from the response payload. */
    std::string sellerName;
    /** Listing lifecycle status returned by the server. */
    std::string status;
    /** Listing title text. */
    std::string title;
    /** Listing description text. */
    std::string description;
    /** Listing price value. */
    double price = 0.0;
    /** Listing category label. */
    std::string category;
};

/**
 * @struct ApiOffer
 * @brief Parsed offer row returned to the client UI.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Aaron Shuan Xie (axie46)
 *
 * Stores one normalized offer row reconstructed from API response fields
 * returned by the server.
 */
struct ApiOffer {
    /** Offer id returned by the server. */
    int offerId = -1;
    /** Listing id associated with the offer. */
    int listingId = -1;
    /** Title of the listing receiving the offer. */
    std::string listingTitle;
    /** Buyer display name returned for the offer row. */
    std::string buyerName;
    /** Seller/owner display name returned for the offer row. */
    std::string ownerName;
    /** Offered price value. */
    double offerPrice = 0.0;
    /** Offer status label normalized for the UI. */
    std::string status;
};

/**
 * @class ApiClient
 * @brief Stateful client for HTTP+JSON API operations.
 *
 * Owns the server connection settings plus cached authentication metadata and
 * exposes typed wrapper methods for each supported marketplace action.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Jasmine Jia Gu (jgu284)
 * @author Samuel Ross Wobschall (swobscha)
 * @author Aaron Shuan Xie (axie46)
 * @author Ashwin Subash (asubash2)
 */
class ApiClient {
public:
    /**
     * @brief Constructs an API client.
     *
     * Stores the target host and port that will be used for all later request
     * round trips made by this client instance.
     * @param host Server hostname/IP.
     * @param port Server TCP port.
     */
    ApiClient(const std::string& host = "127.0.0.1", int port = 9090);

    /**
     * @brief Destroys the API client.
     *
     * Releases any owned HTTP client resources and cached client state.
     */
    ~ApiClient();

    /**
     * @brief Registers a new user account.
     *
     * Sends a registration request and returns the server's success or error
     * message while caching session data on success.
     * @param email New account email.
     * @param password Plaintext password.
     * @param message Output status message returned by the server.
     * @return True when registration succeeds.
     */
    bool registerUser(const std::string& email, const std::string& password, std::string& message);

    /**
     * @brief Logs in and stores session metadata locally.
     *
     * Sends a login request and caches the returned token, email, and user id
     * when the server authenticates successfully.
     * @param email Account email.
     * @param password Plaintext password.
     * @param message Output status message returned by the server.
     * @return True when login succeeds.
     */
    bool login(const std::string& email, const std::string& password, std::string& message);

    /**
     * @brief Logs out and clears client-side auth state.
     *
     * Sends a logout request when possible and clears the locally cached
     * authentication metadata regardless of server response outcome.
     */
    void logout();

    /**
     * @brief Searches listings by keyword, category, and price sort.
     *
     * Sends the search criteria to the server and reconstructs any returned
     * listings into typed rows for the UI.
     * @param keyword Search term.
     * @param out Output collection of parsed listing rows.
     * @param message Output status message returned by the server.
     * @param category Optional category filter.
     * @param priceSort Optional price sort token.
     * @return True when the request succeeds and listings were parsed.
     */
    bool searchListings(const std::string& keyword, std::vector<ApiListing>& out, std::string& message,
                        const std::string& category = "All", const std::string& priceSort = "none");

    /**
     * @brief Gets one recommended listing.
     *
     * Requests a single recommendation from the server and copies the parsed
     * listing into the supplied output object.
     * @param out Output recommendation listing.
     * @param message Output status message returned by the server.
     * @return True when a recommendation is returned successfully.
     */
    bool getRecommendation(ApiListing& out, std::string& message);

    /**
     * @brief Gets the current user's own listings.
     *
     * Requests the authenticated seller's listings and parses them into typed
     * rows for the client.
     * @param out Output collection of owned listings.
     * @param message Output status message returned by the server.
     * @return True when the request succeeds.
     */
    bool getMyListings(std::vector<ApiListing>& out, std::string& message);

    /**
     * @brief Gets the current user's watchlist.
     *
     * Requests the authenticated user's watchlist contents and parses them
     * into typed listing rows.
     * @param out Output collection of watchlist listings.
     * @param message Output status message returned by the server.
     * @return True when the request succeeds.
     */
    bool getWatchlist(std::vector<ApiListing>& out, std::string& message);

    /**
     * @brief Creates a new listing with an optional raw image upload.
     *
     * Sends the seller's listing fields to the server and optionally uploads
     * a local image file through the multipart photo flow.
     * @param title Listing title.
     * @param description Listing description.
     * @param price Listing price.
     * @param category Listing category.
     * @param message Output status message returned by the server.
     * @param file_path Optional local image path to upload.
     * @return True when the listing is created successfully.
     */
    bool createListing(const std::string& title, const std::string& description, double price,
                       const std::string& category, std::string& message,
                       const std::string& file_path = "");

    /**
     * @brief Edits an existing listing.
     *
     * Sends updated listing fields to the server and optionally replaces the
     * associated image metadata or image contents.
     * @param listingId Listing id to update.
     * @param title Updated title.
     * @param description Updated description.
     * @param price Updated price.
     * @param message Output status message returned by the server.
     * @param file_path Optional replacement local file path. An empty string
     *        requests photo removal, while a null pointer leaves the photo unchanged.
     * @return True when the edit succeeds.
     */
    bool editListing(int listingId, const std::string& title, const std::string& description, double price,
                     std::string& message, const std::string* file_path = nullptr);

    /**
     * @brief Updates a listing category.
     *
     * Sends a category change request for one listing owned by the current
     * seller.
     * @param listingId Listing id to retag.
     * @param category New category label.
     * @param message Output status message returned by the server.
     * @return True when the category update succeeds.
     */
    bool tagListing(int listingId, const std::string& category, std::string& message);

    /**
     * @brief Removes a listing.
     *
     * Sends a delete request for one listing owned by the current seller.
     * @param listingId Listing id to remove.
     * @param message Output status message returned by the server.
     * @return True when the listing is removed successfully.
     */
    bool removeListing(int listingId, std::string& message);

    /**
     * @brief Adds a listing to the watchlist.
     *
     * Sends a watchlist-add request for the specified listing id.
     * @param listingId Listing id to watch.
     * @param message Output status message returned by the server.
     * @return True when the listing is added successfully.
     */
    bool addToWatchlist(int listingId, std::string& message);

    /**
     * @brief Removes a listing from the watchlist.
     *
     * Sends a watchlist-removal request for the specified listing id.
     * @param listingId Listing id to remove from the watchlist.
     * @param message Output status message returned by the server.
     * @return True when the listing is removed successfully.
     */
    bool removeFromWatchlist(int listingId, std::string& message);

    /**
     * @brief Makes an offer on a listing.
     *
     * Sends the buyer's offer price for the specified listing.
     * @param listingId Listing id receiving the offer.
     * @param offerPrice Offered price.
     * @param message Output status message returned by the server.
     * @return True when the offer is created successfully.
     */
    bool makeOffer(int listingId, double offerPrice, std::string& message);

    /**
     * @brief Gets incoming offers for the current seller.
     *
     * Requests offer rows targeting the seller's listings and parses them
     * into typed offer records.
     * @param out Output collection of incoming offers.
     * @param message Output status message returned by the server.
     * @return True when the request succeeds.
     */
    bool getIncomingOffers(std::vector<ApiOffer>& out, std::string& message);

    /**
     * @brief Gets outgoing offers for the current buyer.
     *
     * Requests offer rows created by the authenticated buyer and parses them
     * into typed offer records.
     * @param out Output collection of outgoing offers.
     * @param message Output status message returned by the server.
     * @return True when the request succeeds.
     */
    bool getMyOffers(std::vector<ApiOffer>& out, std::string& message);

    /**
     * @brief Accepts an offer.
     *
     * Sends a request to accept one pending offer.
     * @param offerId Offer id to accept.
     * @param message Output status message returned by the server.
     * @return True when the offer is accepted successfully.
     */
    bool acceptOffer(int offerId, std::string& message);

    /**
     * @brief Rejects an offer.
     *
     * Sends a request to reject one pending offer.
     * @param offerId Offer id to reject.
     * @param message Output status message returned by the server.
     * @return True when the offer is rejected successfully.
     */
    bool rejectOffer(int offerId, std::string& message);

    /**
     * @brief Retrieves a listing photo as raw bytes.
     *
     * Requests the listing's raw image payload from the dedicated photo
     * endpoint and copies the returned bytes into the supplied buffer.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @param listingId Listing id whose photo should be fetched.
     * @param outRawBytes Output raw image bytes.
     * @param message Output status message.
     * @return True when the image is fetched successfully.
     */
    bool viewPhotoRaw(int listingId, std::vector<uint8_t>& outRawBytes, std::string& message);

    /**
     * @brief Gets user notifications.
     *
     * Requests the authenticated user's notifications and copies them into the
     * supplied output collection.
     * @param out Output collection of notification messages.
     * @param message Output status message returned by the server.
     * @return True when the request succeeds.
     */
    bool getNotifications(std::vector<std::string>& out, std::string& message);

    /**
     * @brief Bans a user as an admin operation.
     *
     * Sends the admin code and target user id to the server to request a ban.
     * @param adminCode Administrative authorization code.
     * @param userId User id to ban.
     * @param message Output status message returned by the server.
     * @return True when the ban succeeds.
     */
    bool banUser(const std::string& adminCode, int userId, std::string& message);

    /**
     * @brief Gets purchased listings for the current user.
     *
     * Requests previously purchased listing rows and parses them into typed
     * listing records.
     * @param out Output collection of purchased listings.
     * @param message Output status message returned by the server.
     * @return True when the request succeeds.
     */
    bool getMyPurchases(std::vector<ApiListing>& out, std::string& message);

    /**
     * @brief Gets a seller's average rating.
     *
     * Requests the seller's current average rating summary from the server.
     * @param sellerId Seller id whose rating should be fetched.
     * @param averageRating Output average rating string.
     * @param message Output status message returned by the server.
     * @return True when the request succeeds.
     */
    bool getSellerRating(int sellerId, std::string& averageRating, std::string& message);

    /**
     * @brief Checks whether the current user can rate a seller.
     *
     * Sends the listing and seller identifiers so the server can validate
     * rating eligibility.
     * @param listingId Listing id associated with the completed purchase.
     * @param sellerId Seller id to be rated.
     * @param message Output status message returned by the server.
     * @return True when the user is allowed to rate the seller.
     */
    bool canRateSeller(int listingId, int sellerId, std::string& message);

    /**
     * @brief Submits a seller rating.
     *
     * Sends the user's rating and optional review comment for a seller tied
     * to a specific purchased listing.
     * @param listingId Listing id associated with the rating.
     * @param sellerId Seller id being rated.
     * @param rating Rating value to submit.
     * @param comment Optional review comment.
     * @param message Output status message returned by the server.
     * @return True when the rating is submitted successfully.
     */
    bool rateSeller(int listingId, int sellerId, int rating, const std::string& comment, std::string& message);

    /**
     * @brief Returns whether the client currently has a valid session token.
     * @return True when the client is logged in.
     */
    bool isLoggedIn() const;

    /**
     * @brief Returns the cached authenticated email.
     * @return Current authenticated email.
     */
    const std::string& getCurrentEmail() const;

    /**
     * @brief Returns the cached authenticated user id.
     * @return Current authenticated user id, or `-1` when logged out.
     */
    int getCurrentUserId() const;

private:
    std::unique_ptr<httplib::Client> client_;  ///< HTTP client from cpp-httplib.
    std::string token_;                        ///< Session token.
    std::string email_;                        ///< Authenticated email.
    int userId_ = -1;                          ///< Authenticated user ID.

    /**
     * @brief Sends a JSON request to the server and parses the response.
     * @author Muhammad Naheen Mahboob (mmahbo)
     *
     * Centralizes one API round trip so higher-level methods can submit an
     * action plus named fields and receive a parsed response map.
     * @param action API action name.
     * @param fields Request fields (key-value pairs).
     * @param response Output map of response fields.
     * @param needsAuth If true, automatically adds token to request.
     * @return True on success.
     */
    bool sendRequest(const std::string& action,
                     const std::vector<std::pair<std::string, std::string>>& fields,
                     std::unordered_map<std::string, std::string>& response,
                     bool needsAuth = true);

    /**
     * @brief Converts flattened response map into vector of ApiListing.
     * @author Muhammad Naheen Mahboob (mmahbo)
     *
     * Reconstructs listing rows from the flattened response field naming
     * convention used by the marketplace API.
     * @param response Parsed response map.
     * @return List of listings.
     */
    std::vector<ApiListing> parseListings(const std::unordered_map<std::string, std::string>& response);

    /**
     * @brief Converts flattened response map into vector of ApiOffer.
     * @author Muhammad Naheen Mahboob (mmahbo)
     *
     * Reconstructs offer rows from the flattened response field naming
     * convention used by the marketplace API.
     * @param response Parsed response map.
     * @return List of offers.
     */
    std::vector<ApiOffer> parseOffers(const std::unordered_map<std::string, std::string>& response);
};

#endif
