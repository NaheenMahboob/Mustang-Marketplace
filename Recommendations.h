// Recommendations.h
// MustangMarketplace — Group 49
// Story 17

#ifndef RECOMMENDATIONS_H
#define RECOMMENDATIONS_H

#include <string>
#include "Database.h"
#include "User.h"

/**
 * @file Recommendations.h
 * @brief Recommendation helpers driven by the latest watchlisted listing category.
 *
 * Declares the recommendation model and the service method that produces one
 * recommended listing based on recent watchlist activity.
 * @author Samuel Ross Wobschall (swobscha)
 */

/**
 * @class Recommendations
 * @brief Recommendation result model and service methods.
 *
 * Represents the category signal and resulting title associated with one
 * recommendation lookup, and exposes the main recommendation query.
 * @author Samuel Ross Wobschall (swobscha)
 */
class Recommendations {
private:
    /** Latest watchlist-derived category signal stored in this snapshot. */
    std::string category;
    /** Recommended listing title stored in this snapshot. */
    std::string recommended_title;

public:
    /**
     * @brief Constructs recommendation snapshot.
     *
     * Copies the supplied recommendation fields into an in-memory object for
     * simple inspection and transport.
     * @param category Category signal used for the recommendation.
     * @param title Recommended listing title.
     */
    Recommendations(const std::string& category, const std::string& title);

    /** @brief Returns the latest-watchlist category used for recommendation.
     *
     * Provides the category preference captured on this recommendation
     * snapshot.
     * @return Recommendation category string.
     */
    std::string getCategory()          const;
    /** @brief Returns recommended listing title.
     *
     * Provides the recommended listing title stored on this snapshot.
     * @return Recommended listing title.
     */
    std::string getRecommendedTitle() const;

    // Story 17: Get Recommendations
    // PASS: latest watchlist category found -> one TSV listing row
    // PASS: watchlisted items are excluded  -> one TSV listing row
    // PASS: no watchlist category           -> one TSV listing row
    // FAIL: no eligible listings remain     -> "MARKET_EMPTY"
    /**
     * @brief Gets a recommendation using the latest watchlisted category and excludes watched listings.
     *
     * Chooses one eligible active listing, prioritizing the category from the
     * most recent watchlist item while excluding already watched listings.
     * @author Samuel Ross Wobschall (swobscha)
     * @return One TSV row, `MARKET_EMPTY`, or `DB_ERROR`.
    */
    static std::string get();
};

#endif
