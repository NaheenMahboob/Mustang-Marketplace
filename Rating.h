// Rating.h
// MustangMarketplace — Group 49
// Stories 18, 19

#ifndef RATING_H
#define RATING_H
#include <string>
#include "Database.h"
#include "User.h"

/**
 * @file Rating.h
 * @brief Purchase-backed seller rating model and buyer-authenticated rating operation.
 *
 * Declares the seller-rating entity and the purchase-aware service operations
 * used to view and submit seller ratings.
 * @author Aaron Shuan Xie (axie46)
 * @author Ashwin Subash (asubash2)
 */

/**
 * @class Rating
 * @brief Rating entity and rating service methods.
 *
 * Represents one persisted seller rating snapshot and exposes the operations
 * that view rating summaries and validate or submit buyer reviews.
 * @author Aaron Shuan Xie (axie46)
 * @author Ashwin Subash (asubash2)
 */
class Rating {
private:
    /** Persisted rating id for this snapshot. */
    int rating_id;
    /** Listing id tied to the rating. */
    int listing_id;
    /** Buyer id that authored the rating. */
    int buyer_id;
    /** Seller id being rated. */
    int seller_id;
    /** Numeric score value stored with the rating. */
    int rating_value;
    /** Optional free-form review comment. */
    std::string comment;

public:
    /**
     * @brief Constructs a rating object snapshot.
     *
     * Copies the supplied rating row fields into an in-memory object for later
     * inspection without another database query.
     * @param rid Persisted rating id.
     * @param lid Listing id tied to the rating.
     * @param bid Buyer id that submitted the rating.
     * @param sid Seller id being rated.
     * @param val Numeric rating value.
     * @param comment Optional review comment text.
     */
    Rating(int rid, int lid, int bid, int sid, int val, const std::string& comment);

    /** @brief Returns rating id.
     *
     * Provides the persisted identifier for this rating snapshot.
     * @return Rating id value.
     */
    int getRatingId() const;
    /** @brief Returns purchased listing id tied to the review.
     *
     * Provides the listing id associated with this rating snapshot.
     * @return Purchased listing id value.
     */
    int getListingId() const;
    /** @brief Returns buyer id.
     *
     * Provides the buyer id stored on this rating snapshot.
     * @return Buyer id value.
     */
    int getBuyerId() const;
    /** @brief Returns seller id.
     *
     * Provides the seller id stored on this rating snapshot.
     * @return Seller id value.
     */
    int getSellerId() const;
    /** @brief Returns numeric rating value.
     *
     * Provides the numeric score stored on this rating snapshot.
     * @return Rating score value.
     */
    int getRatingValue() const;
    /** @brief Returns optional text comment.
     *
     * Provides the free-form review comment stored on this rating snapshot.
     * @return Rating comment text.
     */
    std::string getComment() const;

    // Story 18: View Seller Rating
    /**
     * @brief Returns seller rating summary formatted as `avg (count)`.
     *
     * Aggregates visible ratings for the target seller and formats the result
     * for direct display in the client.
     * @author Aaron Shuan Xie (axie46)
     * @param seller_id Seller id.
     * @return `Profile Hidden`, `avg (count)`, `No ratings yet`, or `DB Error`.
     */
    static std::string viewSellerRating(int seller_id);

    // Story 19: Rate Seller
    /**
     * @brief Checks whether the current buyer can rate a purchased listing.
     *
     * Validates that the current authenticated buyer completed the purchase
     * and has not already submitted a rating for it.
     * @author Ashwin Subash (asubash2)
     * @param listing_id Purchased listing id.
     * @param seller_id Seller id.
     * @return `OK`, `Already rated this purchase`, `No completed transaction`, or `DB Error`.
     */
    static std::string canRateSeller(int listing_id, int seller_id);

    // Story 19: Rate Seller
    /**
     * @brief Rates a seller for a purchased listing owned by that seller.
     *
     * Inserts a new seller rating for a completed purchase after validating
     * eligibility, score range, and ownership relationships.
     * @author Ashwin Subash (asubash2)
     * @param listing_id Purchased listing id.
     * @param seller_id Seller id.
     * @param rating Numeric rating value.
     * @param comment Optional review text.
     * @return Operation status string.
     */
    static std::string rateSeller(int listing_id, int seller_id, int rating, const std::string& comment);
};

#endif
