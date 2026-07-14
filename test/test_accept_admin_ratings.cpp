/**
 * @file test_accept_admin_ratings.cpp
 * @brief Acceptance tests for moderation and seller-rating flows.
 *
 * Runs grouped acceptance scenarios for Stories 15, 18, and 19 using the
 * shared in-memory acceptance-test harness.
 */

#include "test_accept_helpers.h"

#include "Admin.h"
#include "Rating.h"
#include "User.h"

#include <string>

using namespace std;

/**
 * @brief Executes the admin/ratings acceptance suite.
 *
 * Seeds a fresh in-memory fixture, exercises the admin ban workflow, and then
 * validates seller-rating display, eligibility checks, and rating submission.
 * @param argc Number of command-line arguments.
 * @param argv Command-line arguments, used to harvest an existing file path for photo fixtures.
 * @return Process exit code for the suite.
 */
int main(int argc, char** argv) {
    // Reuse the running binary path as the existing-file fixture required by shared seed data.
    const string photoFixture = (argc > 0) ? argv[0] : "./test_accept_admin_ratings_bin";

    // Prepare a clean fixture before the grouped moderation and rating assertions run.
    AcceptTest::openFreshDb();
    AcceptTest::seedBaseData(photoFixture);
    AcceptTest::beginSuite("MustangMarketplace — Acceptance Tests (Admin / Ratings)");

    AcceptTest::printStoryHeader("Story 15: Ban User");
    // Ban user 2 first, then verify the login and not-found moderation branches.
    AcceptTest::checkContains("15.1 Valid user -> banned",
                              "User banned",
                              Admin::banUser(2));
    AcceptTest::resetSession();
    AcceptTest::checkContains("15.2 Banned user login blocked",
                              "Access Denied",
                              User::login("other@uwo.ca", "pass2"));
    AcceptTest::checkContains("15.3 Unknown user -> User Not Found",
                              "User Not Found",
                              Admin::banUser(999));
    // Restore the shared alternate user so the later rating scenarios can use seller 2.
    AcceptTest::execSql("UPDATE users SET is_banned = 0 WHERE user_id = 2;");
    printf("\n");

    AcceptTest::printStoryHeader("Story 18 and 19: View / Rate Seller");
    // Insert multiple sold and unsold listings to cover rating eligibility, duplicate, and wrong-buyer branches.
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, buyer_id, title, description, price, status) "
        "VALUES (2, 1, 'Sold Item A', 'sold desc', 50.0, 'sold');");
    int soldListing1 = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, buyer_id, title, description, price, status) "
        "VALUES (2, 1, 'Sold Item B', 'sold desc 2', 60.0, 'sold');");
    int soldListing2 = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, buyer_id, title, description, price, status) "
        "VALUES (2, 3, 'Other Buyer Item', 'sold to someone else', 65.0, 'sold');");
    int otherBuyerSoldListing = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, status) "
        "VALUES (2, 'Active Item', 'not sold yet', 70.0, 'active');");
    int activeListing = static_cast<int>(sqlite3_last_insert_rowid(testDB));

    // Authenticate as buyer 1 before checking rating display and submission behavior.
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("18.1 Seller with no ratings reports empty state",
                              "No ratings yet",
                              Rating::viewSellerRating(2));
    AcceptTest::checkContains("18.1B Invalid seller id reports empty state",
                              "No ratings yet",
                              Rating::viewSellerRating(0));
    AcceptTest::checkContains("19.1 Precheck allows unrated purchased listing before submission",
                              "OK",
                              Rating::canRateSeller(soldListing1, 2));
    AcceptTest::checkContains("19.1B Invalid seller id precheck -> No completed transaction",
                              "No completed transaction",
                              Rating::canRateSeller(soldListing1, 0));
    AcceptTest::checkContains("19.2 Buyer can rate purchased listing",
                              "Rating submitted",
                              Rating::rateSeller(soldListing1, 2, 5, "Great seller"));
    AcceptTest::checkContains("19.3 Same purchase cannot be rated twice",
                              "Already rated this purchase",
                              Rating::rateSeller(soldListing1, 2, 4, "Second try"));
    AcceptTest::checkContains("19.4 Precheck blocks already-rated purchase",
                              "Already rated this purchase",
                              Rating::canRateSeller(soldListing1, 2));
    AcceptTest::checkContains("19.5 Precheck allows second unrated purchase",
                              "OK",
                              Rating::canRateSeller(soldListing2, 2));
    AcceptTest::checkContains("19.6 Buyer can rate second purchase from same seller",
                              "Rating submitted",
                              Rating::rateSeller(soldListing2, 2, 4, "Another good sale"));
    // Switch to another user to verify failed rating attempts on someone else's purchase.
    currentSession = {true, 2, "other@uwo.ca"};
    AcceptTest::checkContains("19.7 No completed transaction -> blocked",
                              "No completed transaction",
                              Rating::rateSeller(soldListing1, 1, 4, "Invalid"));
    // Return to buyer 1 for the remaining negative checks and display assertions.
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("19.8 Cannot rate listing bought by another buyer",
                              "No completed transaction",
                              Rating::rateSeller(otherBuyerSoldListing, 2, 5, "Not mine"));
    AcceptTest::checkContains("19.9 Cannot rate active listing",
                              "No completed transaction",
                              Rating::rateSeller(activeListing, 2, 5, "Too early"));
    AcceptTest::checkContains("19.10 Invalid rating value blocked",
                              "Invalid rating",
                              Rating::rateSeller(soldListing2, 2, 6, "Too high"));
    AcceptTest::checkContains("19.11 Missing listing cannot be rated",
                              "No completed transaction",
                              Rating::rateSeller(9999, 2, 5, "Missing"));
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, buyer_id, title, description, price, status) "
        "VALUES (2, 1, 'Sold Item C', 'sold desc 3', 40.0, 'sold');");
    int soldListing3 = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    AcceptTest::checkContains("19.12 Boundary rating value 1 is accepted",
                              "Rating submitted",
                              Rating::rateSeller(soldListing3, 2, 1, "Low but valid"));
    AcceptTest::resetSession();
    AcceptTest::checkContains("19.13 Unauthenticated canRateSeller -> Session Expired",
                              "Session Expired",
                              Rating::canRateSeller(soldListing2, 2));
    AcceptTest::checkContains("19.14 Unauthenticated rateSeller -> Session Expired",
                              "Session Expired",
                              Rating::rateSeller(soldListing2, 2, 5, "Blocked"));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("18.2 Seller average rating shown to one decimal place",
                              "3.3 (3)",
                              Rating::viewSellerRating(2));
    AcceptTest::checkContains("18.3 Seller 1 still has no ratings",
                              "No ratings yet",
                              Rating::viewSellerRating(1));
    // Re-ban seller 2 to verify the Story 18 "Profile Hidden" branch.
    AcceptTest::checkContains("18.4 Banning seller succeeds",
                              "User banned",
                              Admin::banUser(2));
    AcceptTest::checkContains("18.5 Banned seller returns Profile Hidden",
                              "Profile Hidden",
                              Rating::viewSellerRating(2));
    printf("\n");

    // Release the in-memory DB before returning the aggregated suite result.
    AcceptTest::closeDb();
    return AcceptTest::finishSuite();
}
