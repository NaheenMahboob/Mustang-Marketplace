// Listing.h
// MustangMarketplace — Group 49
// Stories 5, 6, 7, 10, 11, 14, 19

#ifndef LISTING_H
#define LISTING_H

#include <string>
#include "Database.h"
#include "User.h"

/**
 * @file Listing.h
 * @brief Listing domain model and listing-related business operations.
 *
 * Declares the marketplace listing entity together with the main create,
 * search, view, edit, and delete operations used by the application.
 */

/**
 * @class Listing
 * @brief Listing entity and listing service methods.
 *
 * Represents one listing snapshot and exposes the business actions that load
 * or mutate listing data for buyers and sellers.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Jasmine Jia Gu (jgu284)
 * @author Samuel Ross Wobschall (swobscha)
 * @author Ashwin Subash (asubash2)
 */
class Listing {
private:
    /** Persisted listing id for this snapshot. */
    int listing_id;
    /** Seller id that owns this listing snapshot. */
    int seller_id;
    /** Listing title text. */
    std::string title;
    /** Listing description text. */
    std::string description;
    /** Listing price value. */
    double price;
    /** Listing category label. */
    std::string category;
    /** Listing lifecycle status text. */
    std::string status;

public:
    /**
     * @brief Constructs a listing object snapshot.
     *
     * Copies the supplied listing fields into an in-memory object that can be
     * passed through the UI and service layers.
     * @param lid Persisted listing id.
     * @param sid Seller id that owns the listing.
     * @param title Listing title text.
     * @param desc Listing description text.
     * @param price Listing price value.
     * @param category Listing category label.
     * @param status Listing lifecycle status.
     */
    Listing(int lid, int sid, const std::string& title, const std::string& desc,
            double price, const std::string& category, const std::string& status);

    /** @brief Returns listing id.
     *
     * Provides the persisted identifier for this listing snapshot.
     * @return Listing id value.
     */
    int getListingId() const;
    /** @brief Returns seller id.
     *
     * Provides the id of the seller who owns this listing snapshot.
     * @return Seller id value.
     */
    int getSellerId()    const;
    /** @brief Returns title.
     *
     * Provides the display title stored on this listing snapshot.
     * @return Listing title text.
     */
    std::string getTitle()       const;
    /** @brief Returns description.
     *
     * Provides the description text stored on this listing snapshot.
     * @return Listing description text.
     */
    std::string getDescription() const;
    /** @brief Returns price.
     *
     * Provides the current price value stored on this listing snapshot.
     * @return Listing price.
     */
    double getPrice()       const;
    /** @brief Returns category.
     *
     * Provides the current category label stored on this listing snapshot.
     * @return Listing category text.
     */
    std::string getCategory()    const;
    /** @brief Returns listing status.
     *
     * Provides the lifecycle status associated with this listing snapshot.
     * @return Listing status text.
     */
    std::string getStatus()      const;

    // Story 5: Create Listing
    // PASS: valid data       -> "Listing created"
    // FAIL: empty title/desc -> "Invalid input"
    // FAIL: negative price   -> "Invalid input"
    /**
     * @brief Creates a listing for the current session user.
     *
     * Validates the submitted listing fields and inserts a new active listing
     * owned by the authenticated session user.
     * @author Samuel Ross Wobschall (swobscha)
     * @param title Listing title.
     * @param description Listing description.
     * @param price Listing price.
     * @param category Listing category.
     * @param out_listing_id Optional output listing id on success.
     * @return Operation status string.
     */
    static std::string create(const std::string& title, const std::string& description, double price,
                              const std::string& category, int* out_listing_id = nullptr);

    // Story 6: Delete Listing
    // PASS: owner deletes    -> "Listing removed"
    // FAIL: not the seller   -> "Access Denied"
    // FAIL: already gone     -> "Item Not Found"
    /**
     * @brief Soft-deletes a listing owned by the current session user.
     *
     * Marks the targeted listing as removed when the current session user owns
     * it and the listing is still available to modify.
     * @author Ashwin Subash (asubash2)
     * @param listing_id Listing id.
     * @return Operation status string.
     */
    static std::string remove(int listing_id);

    // Story 7 and 11: Search Listings / Filtering
    // PASS: keyword matches  -> Listing TSV
    // FAIL: no matches       -> "NO_RESULTS"
    /**
     * @brief Searches active listings and returns TSV payload.
     *
     * Queries active listings using optional keyword, category, and price-sort
     * filters, then serializes matching rows into the shared TSV wire format.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @author Jasmine Jia Gu (jgu284)
     * @param keyword Search term; blank means browse filtered active listings.
     * @param category Category filter, or `All` for no category filter.
     * @param price_sort Price ordering token: `none`, `asc`, or `desc`.
     * @return TSV payload, `NO_RESULTS`, or `DB Error`.
     */
    static std::string search(const std::string& keyword,
                              const std::string& category = "All",
                              const std::string& price_sort = "none");

    /**
     * @brief Returns seller's own listings in TSV format.
     *
     * Loads the listings owned by the specified seller and serializes them
     * into the same TSV shape used by other listing views.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @param seller_id Seller id.
     * @return TSV payload, empty string, or `DB Error`.
     */
    static std::string view(int seller_id);

    // Supports Story 19: My Purchases + Rate Seller
    /**
     * @brief Returns buyer purchases in TSV format.
     *
     * Loads completed purchases for the specified buyer and serializes them in
     * a listing-compatible TSV format for client reuse.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @author Jasmine Jia Gu (jgu284)
     * @author Samuel Ross Wobschall (swobscha)
     * @author Ashwin Subash (asubash2)
     * @param buyer_id Buyer id.
     * @return TSV payload, empty string, or `DB Error`.
     */
    static std::string viewPurchases(int buyer_id);

    // Story 10: Tag Listing
    // PASS: valid category   -> "Category assigned"
    // FAIL: empty/invalid    -> "Selection Required"
    // FAIL: not the seller   -> "Access Denied"
    // FAIL: not found        -> "Item Not Found"
    /**
     * @brief Updates listing category for owner.
     *
     * Reassigns the category on an editable listing after validating the new
     * category and confirming ownership.
     * @author Ashwin Subash (asubash2)
     * @param listing_id Listing id.
     * @param category Canonical category text.
     * @return Operation status string.
     */
    static std::string tag(int listing_id, const std::string& category);

    // Story 14: Edit Listing
    // PASS: valid edits      -> "Listing updated"
    // FAIL: not the seller   -> "Access Denied"
    // FAIL: invalid input    -> "Invalid input"
    // FAIL: not found        -> "Item Not Found"
    /**
     * @brief Edits listing details for owner.
     *
     * Applies updated title, description, and price fields to an editable
     * listing after validating ownership and input values.
     * @author Ashwin Subash (asubash2)
     * @param listing_id Listing id.
     * @param title New title.
     * @param description New description.
     * @param price New price.
     * @return Operation status string.
     */
    static std::string edit(int listing_id, const std::string& title,
                       const std::string& description, double price);
};

#endif
