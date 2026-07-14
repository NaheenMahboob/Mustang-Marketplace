// Watchlist.h
// MustangMarketplace — Group 49
// Stories 13, 17

#ifndef WATCHLIST_H
#define WATCHLIST_H

#include <string>
#include "Database.h"
#include "User.h"

/**
 * @file Watchlist.h
 * @brief Watchlist model and add/remove/view operations.
 *
 * Declares the watchlist entity together with the business operations used to
 * add, remove, and inspect saved listings for a user.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */

/**
 * @class Watchlist
 * @brief Watchlist entry entity and service methods.
 *
 * Represents one saved watchlist link and provides the service methods that
 * manage watchlist persistence and retrieval.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */
class Watchlist {
private:
    /** Persisted watchlist row id for this snapshot. */
    int watchlist_id;
    /** User id that owns this watchlist snapshot. */
    int user_id;
    /** Listing id saved in this watchlist snapshot. */
    int listing_id;

public:
    /**
     * @brief Constructs a watchlist entry snapshot.
     *
     * Copies the supplied watchlist row fields into an in-memory object for
     * simple inspection and transport.
     * @param wid Persisted watchlist row id.
     * @param uid User id that owns the watchlist row.
     * @param lid Listing id saved in the watchlist row.
     */
    Watchlist(int wid, int uid, int lid);

    /** @brief Returns watchlist row id.
     *
     * Provides the persisted identifier for this watchlist snapshot.
     * @return Watchlist row id value.
     */
    int getWatchlistId() const;
    /** @brief Returns owning user id.
     *
     * Provides the user id associated with this watchlist snapshot.
     * @return Owning user id value.
     */
    int getUserId()      const;
    /** @brief Returns watched listing id.
     *
     * Provides the listing id stored by this watchlist snapshot.
     * @return Watched listing id value.
     */
    int getListingId() const;

    // Story 13: Add to Watchlist
    // PASS: not yet saved  -> "Added to Watchlist"
    // FAIL: already saved  -> "Already on Watchlist"
    /**
     * @brief Adds a listing to current user's watchlist.
     *
     * Creates a watchlist link for the authenticated user when the listing is
     * not already saved.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @param listing_id Listing id.
     * @return Operation status string.
     */
    static std::string add(int listing_id);
    // Supports Stories 13 and 17
    /**
     * @brief Removes a listing from current user's watchlist.
     *
     * Deletes the saved watchlist link for the authenticated user and target
     * listing when it exists.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @param listing_id Listing id.
     * @return Operation status string.
     */
    static std::string remove(int listing_id);

    // Supports Stories 13 and 17
    /**
     * @brief Returns watchlist listings in TSV format.
     *
     * Loads the specified user's saved listings and serializes them into the
     * shared TSV listing format consumed by the client.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @param user_id User id.
     * @return TSV payload, empty string, or `DB Error`.
     */
    static std::string view(int user_id);
};

#endif
