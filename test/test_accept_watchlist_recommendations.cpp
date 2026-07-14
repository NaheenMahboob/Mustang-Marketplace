/**
 * @file test_accept_watchlist_recommendations.cpp
 * @brief Acceptance tests for watchlist management and recommendations.
 *
 * Runs grouped acceptance scenarios for Stories 13 and 17 using the shared
 * in-memory acceptance-test harness.
 */

#include "test_accept_helpers.h"

#include "Listing.h"
#include "Recommendations.h"
#include "Watchlist.h"

#include <string>

using namespace std;

/**
 * @brief Executes the watchlist/recommendations acceptance suite.
 *
 * Seeds a fresh in-memory fixture, exercises watchlist add/remove/view flows,
 * and then verifies the recommendation engine's category-history behavior.
 * @param argc Number of command-line arguments.
 * @param argv Command-line arguments, used to harvest an existing file path for photo fixtures.
 * @return Process exit code for the suite.
 */
int main(int argc, char** argv) {
    // Reuse the running binary path as the existing-file fixture required by shared seed data.
    const string photoFixture = (argc > 0) ? argv[0] : "./test_accept_watchlist_recommendations_bin";

    // Prepare a clean fixture before running the grouped watchlist and recommendation scenarios.
    AcceptTest::openFreshDb();
    AcceptTest::seedBaseData(photoFixture);
    AcceptTest::beginSuite("MustangMarketplace — Acceptance Tests (Watchlist / Recommendations)");

    AcceptTest::printStoryHeader("Story 13: Watchlist");
    // Authenticate as user 1 before exercising personalized watchlist behavior.
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("13.1 New item -> Added to Watchlist",
                              "Added to Watchlist",
                              Watchlist::add(2));
    AcceptTest::checkContains("13.2 Duplicate item -> Already on Watchlist",
                              "Already on Watchlist",
                              Watchlist::add(2));
    AcceptTest::checkContains("13.3 View shows saved listing",
                              "Physics Notes",
                              Watchlist::view(1));
    AcceptTest::checkContains("13.4 Remove existing item -> Removed from Watchlist",
                              "Removed from Watchlist",
                              Watchlist::remove(2));
    AcceptTest::checkContains("13.5 Remove absent item -> Removed from Watchlist",
                              "Removed from Watchlist",
                              Watchlist::remove(2));
    currentSession.clear();
    AcceptTest::checkContains("13.6 Unauthenticated add -> Session Expired",
                              "Session Expired",
                              Watchlist::add(2));
    AcceptTest::checkContains("13.7 Unauthenticated remove -> Session Expired",
                              "Session Expired",
                              Watchlist::remove(2));
    printf("\n");

    AcceptTest::printStoryHeader("Story 17: Recommendations");
    // Insert a richer set of active listings so recommendation ordering and category switching can be observed.
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, category, status) "
        "VALUES (2, 'Organic Chemistry Guide', 'lab notes', 22.0, 'Books', 'active');");
    int booksMiddleListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, category, status) "
        "VALUES (2, 'Discrete Math Workbook', 'practice problems', 18.0, 'Books', 'active');");
    int booksNewestListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, category, status) "
        "VALUES (2, 'Winter Coat', 'warm jacket', 55.0, 'Clothing', 'active');");
    int clothingListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, category, status) "
        "VALUES (2, 'Graphing Calculator', 'TI model', 70.0, 'Electronics', 'active');");

    // Start from the buyer account; the latest watchlist entry becomes the recommendation driver.
    currentSession = {true, 1, "test@uwo.ca"};
    // Seed an initial Books category driver so the recommendation engine starts in the intended branch.
    AcceptTest::checkContains("17.0 Seed latest watchlist category with Books listing",
                              "Added to Watchlist",
                              Watchlist::add(2));
    AcceptTest::checkContains("17.1 Watchlisted items are excluded from matching recommendations",
                              "Discrete Math Workbook",
                              AcceptTest::recommendationTitleFromTsv(Recommendations::get()));
    // Repeated add/remove cycles walk the engine through every same-category exclusion branch.
    AcceptTest::checkContains("17.2 Watchlisting newest same-category item succeeds",
                              "Added to Watchlist",
                              Watchlist::add(booksNewestListingId));
    AcceptTest::checkContains("17.3 Newest watched item filtered out -> next newest same-category item shown",
                              "Organic Chemistry Guide",
                              AcceptTest::recommendationTitleFromTsv(Recommendations::get()));
    AcceptTest::checkContains("17.4 Removing newest watched item succeeds",
                              "Removed from Watchlist",
                              Watchlist::remove(booksNewestListingId));
    AcceptTest::checkContains("17.5 Older watched same-category item -> newest unwatched same-category shown",
                              "Discrete Math Workbook",
                              AcceptTest::recommendationTitleFromTsv(Recommendations::get()));
    AcceptTest::checkContains("17.6 Watching middle same-category item succeeds",
                              "Added to Watchlist",
                              Watchlist::add(booksMiddleListingId));
    AcceptTest::checkContains("17.7 Latest watched middle item -> newest unwatched same-category item shown",
                              "Discrete Math Workbook",
                              AcceptTest::recommendationTitleFromTsv(Recommendations::get()));
    AcceptTest::checkContains("17.8 Watching remaining newest same-category item succeeds",
                              "Added to Watchlist",
                              Watchlist::add(booksNewestListingId));
    AcceptTest::checkContains("17.9 All same-category listings watched -> Market Empty",
                              "MARKET_EMPTY",
                              AcceptTest::recommendationTitleFromTsv(Recommendations::get()));

    // Clearing the watchlist lets the suite verify fallback behavior for a new latest category.
    AcceptTest::execSql("DELETE FROM watchlist WHERE user_id = 1;");
    AcceptTest::checkContains("17.10 Newer watchlist item changes category",
                              "Added to Watchlist",
                              Watchlist::add(clothingListingId));
    AcceptTest::checkContains("17.11 Only watched item in latest category -> Market Empty",
                              "MARKET_EMPTY",
                              AcceptTest::recommendationTitleFromTsv(Recommendations::get()));

    // Rebuild history with an older Books item plus a newer Clothing item to test deleted-driver cleanup.
    AcceptTest::execSql("DELETE FROM watchlist WHERE user_id = 1;");
    AcceptTest::checkContains("17.12 Older same-category item can be watchlisted",
                              "Added to Watchlist",
                              Watchlist::add(booksMiddleListingId));
    AcceptTest::checkContains("17.13 Deleted newest watchlist item can still drive recommendations",
                              "Added to Watchlist",
                              Watchlist::add(clothingListingId));
    // Switch to seller 2 so delete actions are authorized on the inserted listings.
    currentSession = {true, 2, "other@uwo.ca"};
    AcceptTest::checkContains("17.14 Deleting non-driver listing succeeds",
                              "Listing removed",
                              Listing::remove(booksMiddleListingId));
    AcceptTest::checkEqual("17.15 Non-driver deleted watchlist row is removed immediately",
                           "0",
                           to_string(AcceptTest::scalarInt(
                               "SELECT COUNT(*) FROM watchlist WHERE user_id=1 AND listing_id=" +
                               to_string(booksMiddleListingId) + ";")));
    currentSession = {true, 2, "other@uwo.ca"};
    AcceptTest::checkContains("17.16 Deleting newest driver listing succeeds",
                              "Listing removed",
                              Listing::remove(clothingListingId));
    // Switch back to buyer 1 to observe how recommendation fallback changes after deletions.
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("17.17 Deleted newest watchlist item keeps prior category signal",
                              "MARKET_EMPTY",
                              AcceptTest::recommendationTitleFromTsv(Recommendations::get()));
    AcceptTest::checkContains("17.18 Adding newer watchlist item cleans up deleted previous driver",
                              "Added to Watchlist",
                              Watchlist::add(booksNewestListingId));
    AcceptTest::checkEqual("17.19 Deleted prior latest watchlist row is unlinked after newer add",
                           "0",
                           to_string(AcceptTest::scalarInt(
                               "SELECT COUNT(*) FROM watchlist WHERE user_id=1 AND listing_id=" +
                               to_string(clothingListingId) + ";")));
    AcceptTest::checkContains("17.20 Recommendation now follows new latest watchlist category",
                              "Physics Notes",
                              AcceptTest::recommendationTitleFromTsv(Recommendations::get()));

    // Removing all watchlist history should trigger the global newest-active fallback path.
    AcceptTest::execSql("DELETE FROM watchlist WHERE user_id = 1;");
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("17.21 No watchlist history -> newest active listing shown",
                              "Graphing Calculator",
                              AcceptTest::recommendationTitleFromTsv(Recommendations::get()));
    // Deleting every active listing verifies the empty-market sentinel branch.
    AcceptTest::execSql("UPDATE listings SET status = 'deleted';");
    AcceptTest::checkContains("17.22 No active listings -> Market Empty",
                              "MARKET_EMPTY",
                              Recommendations::get());
    currentSession.clear();
    AcceptTest::checkContains("17.23 Unauthenticated recommendation -> DB_ERROR",
                              "DB_ERROR",
                              Recommendations::get());
    printf("\n");

    // Release the in-memory DB before returning the aggregated suite result.
    AcceptTest::closeDb();
    return AcceptTest::finishSuite();
}
