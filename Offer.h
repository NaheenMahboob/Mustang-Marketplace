// Offer.h
// MustangMarketplace — Group 49
// Stories 8, 9

#ifndef OFFER_H
#define OFFER_H

#include <optional>
#include <string>
#include <vector>
#include "Database.h"

/**
 * @file Offer.h
 * @brief Offer model and incoming/outgoing offer service operations.
 *
 * Declares offer-related row types and the business operations used to create,
 * inspect, accept, and reject offers on listings.
 * @author Aaron Shuan Xie (axie46)
 */

/** @struct IncomingOfferRow
 *  @brief Row from Offer::incoming for seller-facing responses.
 *  @author Aaron Shuan Xie (axie46)
 *
 * Stores one normalized seller-facing offer row reconstructed from database
 * query results.
 */
struct IncomingOfferRow {
    /** Offer id for the incoming offer row. */
    int offer_id = -1;
    /** Listing id tied to the offer row. */
    int listing_id = -1;
    /** Title of the listing that received the offer. */
    std::string listing_title;
    /** Buyer display name shown to the seller. */
    std::string buyer_name;
    /** Offered price value. */
    double offer_price = 0.0;
    /** Current offer status. */
    std::string status;
};

/** @struct OutgoingOfferRow
 *  @brief Row from Offer::outgoing for buyer-facing responses.
 *  @author Aaron Shuan Xie (axie46)
 *
 * Stores one normalized buyer-facing offer row reconstructed from database
 * query results.
 */
struct OutgoingOfferRow {
    /** Offer id for the outgoing offer row. */
    int offer_id = -1;
    /** Listing id tied to the offer row. */
    int listing_id = -1;
    /** Title of the listing that received the offer. */
    std::string listing_title;
    /** Buyer display name carried in the row. */
    std::string buyer_name;
    /** Seller display name shown to the buyer. */
    std::string seller_name;
    /** Offered price value. */
    double offer_price = 0.0;
    /** Current offer status. */
    std::string status;
};

/**
 * @class Offer
 * @brief Offer entity and offer lifecycle methods.
 *
 * Represents one offer snapshot and provides the service operations that drive
 * offer submission and seller response flows.
 * @author Aaron Shuan Xie (axie46)
 */
class Offer {
private:
    /** Persisted offer id for this snapshot. */
    int offer_id;
    /** Listing id associated with this offer snapshot. */
    int listing_id;
    /** Buyer id that created the offer. */
    int buyer_id;
    /** Seller id receiving the offer. */
    int seller_id;
    /** Offered price stored on the snapshot. */
    double offer_price;
    /** Current lifecycle status of the offer. */
    std::string status;

public:
    /**
     * @brief Constructs an offer object snapshot.
     *
     * Copies the supplied offer fields into an in-memory object that can be
     * inspected without another database query.
     * @param oid Persisted offer id.
     * @param lid Listing id tied to the offer.
     * @param bid Buyer id that created the offer.
     * @param sid Seller id that received the offer.
     * @param price Offered price value.
     * @param status Offer status text.
     */
    Offer(int oid, int lid, int bid, int sid, double price, const std::string& status);

    /** @brief Returns offer id.
     *
     * Provides the persisted identifier for this offer snapshot.
     * @return Offer id value.
     */
    int getOfferId() const;
    /** @brief Returns listing id.
     *
     * Provides the listing id associated with this offer snapshot.
     * @return Listing id value.
     */
    int getListingId() const;
    /** @brief Returns buyer id.
     *
     * Provides the buyer id associated with this offer snapshot.
     * @return Buyer id value.
     */
    int getBuyerId() const;
    /** @brief Returns seller id.
     *
     * Provides the seller id associated with this offer snapshot.
     * @return Seller id value.
     */
    int getSellerId() const;
    /** @brief Returns offer price.
     *
     * Provides the proposed price stored on this offer snapshot.
     * @return Offer price value.
     */
    double getOfferPrice() const;
    /** @brief Returns offer status text.
     *
     * Provides the current lifecycle status of this offer snapshot.
     * @return Offer status string.
     */
    std::string getStatus() const;

    // Story 8: Make Offer
    // PASS: creates pending offer linked buyer/seller -> "Offer submitted"
    // FAIL: amount <= 0 -> "Invalid Offer"
    // FAIL: listing missing/deleted/sold -> "Item Not Found"
    // FAIL: buyer offers on own listing -> "Access Denied"
    /**
     * @brief Creates a pending offer on an active listing.
     *
     * Validates the request and inserts a new pending offer tied to the
     * current buyer and target listing when the listing is eligible.
     * @author Aaron Shuan Xie (axie46)
     * @param listing_id Target listing id.
     * @param offer_price Proposed price.
     * @return Operation status string.
     */
    static std::string make(int listing_id, double offer_price);

    // Supports Stories 8 and 9: Offer inbox/outgoing history flows
    /**
     * @brief Returns pending/processed offers received by seller.
     *
     * Loads the seller-facing offer inbox and normalizes each row into typed
     * structures for further response shaping.
     * @author Aaron Shuan Xie (axie46)
     * @param seller_id Seller id.
     * @return Parsed rows, or nullopt on DB failure.
     */
    static std::optional<std::vector<IncomingOfferRow>> incoming(int seller_id);

    // Supports Stories 8 and 9: Offer inbox/outgoing history flows
    /**
     * @brief Returns offers made by buyer.
     *
     * Loads the buyer's outgoing offer history and normalizes each row into
     * typed structures for further response shaping.
     * @author Aaron Shuan Xie (axie46)
     * @param buyer_id Buyer id.
     * @return Parsed rows, or nullopt on DB failure.
     */
    static std::optional<std::vector<OutgoingOfferRow>> outgoing(int buyer_id);

    // Story 9: Accept Offer
    /**
     * @brief Accepts a pending offer and applies related state transitions.
     *
     * Marks the chosen offer as accepted and applies any accompanying listing,
     * peer-offer, and notification side effects required by the workflow.
     * @author Aaron Shuan Xie (axie46)
     * @param offer_id Offer id.
     * @return Operation status string.
     */
    static std::string accept(int offer_id);
    // Story 9: Reject Offer
    /**
     * @brief Rejects a pending offer.
     *
     * Marks the chosen offer as rejected after verifying that the current
     * seller is allowed to respond to it.
     * @author Aaron Shuan Xie (axie46)
     * @param offer_id Offer id.
     * @return Operation status string.
     */
    static std::string reject(int offer_id);
};

#endif
