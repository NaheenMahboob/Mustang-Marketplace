/**
 * @file test_accept_offers_notifications.cpp
 * @brief Acceptance tests for offer creation, seller responses, and notifications.
 *
 * Runs grouped acceptance scenarios for Stories 8 and 9 using the shared
 * acceptance-test harness.
 */

#include "test_accept_helpers.h"

#include "Notification.h"
#include "Offer.h"
#include "Watchlist.h"

#include <optional>
#include <string>
#include <vector>

using namespace std;

/**
 * @brief Executes the offers/notifications acceptance suite.
 *
 * Seeds a fresh in-memory fixture, exercises the make-offer and accept/reject
 * workflows, and verifies related notification and DB state changes.
 * @param argc Number of command-line arguments.
 * @param argv Command-line arguments, used to harvest an existing file path for photo fixtures.
 * @return Process exit code for the suite.
 */
int main(int argc, char** argv) {
    // Reuse the running binary path as the existing-file fixture required by shared seed data.
    const string photoFixture = (argc > 0) ? argv[0] : "./test_accept_offers_notifications_bin";

    // Prepare a clean fixture before running the grouped offer scenarios.
    AcceptTest::openFreshDb();
    AcceptTest::seedBaseData(photoFixture);
    AcceptTest::beginSuite("MustangMarketplace — Acceptance Tests (Offers / Notifications)");

    AcceptTest::printStoryHeader("Story 8: Make Offer");
    // Insert a dedicated active listing owned by user 2 so the buyer can submit offers against it.
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, status) "
        "VALUES (2, 'Offer Test Item', 'desc', 30.0, 'active');");
    int offerListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));

    // Buyer 1 makes the canonical valid offer used by the later response-flow tests.
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("8.1 Valid offer -> Offer submitted",
                              "Offer submitted",
                              Offer::make(offerListingId, 25.0));
    // Inspect outgoing history to confirm the new offer is visible to the buyer.
    optional<vector<OutgoingOfferRow>> outgoing = Offer::outgoing(1);
    AcceptTest::checkTrue("8.2 Outgoing offers include pending offer",
                          outgoing.has_value() && !outgoing->empty() && outgoing->front().listing_id == offerListingId,
                          "Expected latest outgoing row for the new offer");
    // Inspect notifications to confirm the seller saw the offer side effect.
    Notification::ViewData sellerNotes = Notification::view(2);
    AcceptTest::checkTrue("8.3 Seller receives new-offer notification",
                          sellerNotes.ok && !sellerNotes.notes.empty() &&
                              sellerNotes.notes.front().find("Offer: $25.00") != string::npos,
                          "Expected seller notification for submitted offer");
    AcceptTest::checkContains("8.4 Invalid price -> Invalid Offer",
                              "Invalid Offer",
                              Offer::make(offerListingId, 0.0));
    AcceptTest::checkContains("8.5 Missing listing -> Item Not Found",
                              "Item Not Found",
                              Offer::make(9999, 10.0));
    currentSession = {true, 2, "other@uwo.ca"};
    AcceptTest::checkContains("8.6 Seller offers on own listing -> Access Denied",
                              "Access Denied",
                              Offer::make(offerListingId, 20.0));
    // Insert sold and deleted listings explicitly to verify ineligible-target branches.
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, status) "
        "VALUES (2, 'Sold Offer Item', 'desc', 30.0, 'sold');");
    int soldListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("8.7 Sold listing -> Item Not Found",
                              "Item Not Found",
                              Offer::make(soldListingId, 20.0));
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, status) "
        "VALUES (2, 'Deleted Offer Item', 'desc', 30.0, 'deleted');");
    int deletedListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    AcceptTest::checkContains("8.8 Deleted listing -> Item Not Found",
                              "Item Not Found",
                              Offer::make(deletedListingId, 20.0));
    currentSession.clear();
    AcceptTest::checkContains("8.9 Unauthenticated offer -> Session Expired",
                              "Session Expired",
                              Offer::make(offerListingId, 20.0));
    printf("\n");

    AcceptTest::printStoryHeader("Story 9: Accept / Reject Offer");
    // Recover the offer id created above so seller response tests target the real row.
    int offerId = AcceptTest::scalarInt(
        "SELECT offer_id FROM offers WHERE listing_id = " + to_string(offerListingId) + " AND buyer_id = 1 LIMIT 1;");
    // Seller inbox should surface the pending offer before the seller responds.
    optional<vector<IncomingOfferRow>> incoming = Offer::incoming(2);
    AcceptTest::checkTrue("9.1 Incoming offers show seller pending offer",
                          incoming.has_value() && !incoming->empty() && incoming->front().offer_id == offerId,
                          "Expected pending incoming offer for seller");

    // Switch to the seller account to accept the original pending offer.
    currentSession = {true, 2, "other@uwo.ca"};
    AcceptTest::checkContains("9.2 Seller accepts offer -> Offer accepted",
                              "Offer accepted",
                              Offer::accept(offerId));
    // Validate the listing state transition and buyer linkage caused by acceptance.
    AcceptTest::checkEqual("9.3 Accepted offer marks listing sold",
                           "sold",
                           AcceptTest::scalarText("SELECT status FROM listings WHERE listing_id = " +
                                                  to_string(offerListingId) + ";"));
    AcceptTest::checkEqual("9.4 Accepted offer records buyer on listing",
                           "1",
                           to_string(AcceptTest::scalarInt("SELECT buyer_id FROM listings WHERE listing_id = " +
                                                           to_string(offerListingId) + ";")));
    AcceptTest::checkEqual("9.5 Accepted offer keeps buyer watchlist link",
                           "1",
                           to_string(AcceptTest::scalarInt("SELECT COUNT(*) FROM watchlist WHERE user_id = 1 AND listing_id = " +
                                                           to_string(offerListingId) + ";")));
    // Notifications should now include the acceptance message for the buyer.
    Notification::ViewData buyerNotesAfterAccept = Notification::view(1);
    AcceptTest::checkTrue("9.6 Buyer receives accepted notification",
                          buyerNotesAfterAccept.ok && !buyerNotesAfterAccept.notes.empty() &&
                              buyerNotesAfterAccept.notes.front().find("accepted") != string::npos,
                          "Expected accept notification for buyer");

    // Create a second listing and offer dedicated to the rejection branch.
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, status) "
        "VALUES (2, 'Reject Test Item', 'desc', 20.0, 'active');");
    int rejectListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    Offer::make(rejectListingId, 15.0);
    int rejectOfferId = AcceptTest::scalarInt(
        "SELECT offer_id FROM offers WHERE listing_id = " + to_string(rejectListingId) + " LIMIT 1;");

    // Seller rejects the second offer, then buyer and seller denial cases are checked.
    currentSession = {true, 2, "other@uwo.ca"};
    AcceptTest::checkContains("9.7 Seller rejects offer -> Offer rejected",
                              "Offer rejected",
                              Offer::reject(rejectOfferId));

    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("9.8 Non-seller tries to accept -> Access Denied",
                              "Access Denied",
                              Offer::accept(rejectOfferId));

    currentSession = {true, 2, "other@uwo.ca"};
    AcceptTest::checkContains("9.9 Accept already-closed offer -> Offer Not Found",
                              "Offer Not Found",
                              Offer::accept(offerId));
    currentSession = {true, 2, "other@uwo.ca"};
    AcceptTest::checkContains("9.10 Reject already-accepted offer -> Offer Not Found",
                              "Offer Not Found",
                              Offer::reject(offerId));

    // Verify ordering and status-history visibility after the rejection branch completes.
    optional<vector<IncomingOfferRow>> incomingAfterResponses = Offer::incoming(2);
    bool stillSeesAcceptedOrRejected = false;
    if (incomingAfterResponses.has_value()) {
        for (const auto& row : *incomingAfterResponses) {
            if (row.offer_id == offerId || row.offer_id == rejectOfferId) stillSeesAcceptedOrRejected = true;
        }
    }
    AcceptTest::checkTrue("9.11 Incoming offers exclude accepted/rejected items",
                          !stillSeesAcceptedOrRejected,
                          "Expected seller inbox to show only pending offers");
    Notification::ViewData buyerNotes = Notification::view(1);
    AcceptTest::checkTrue("9.12 Notifications are newest-first after reject",
                          buyerNotes.ok && buyerNotes.notes.size() >= 2 &&
                              buyerNotes.notes.front().find("rejected") != string::npos,
                          "Expected newest buyer notification to be the rejection");
    optional<vector<OutgoingOfferRow>> outgoingAfterReject = Offer::outgoing(1);
    // Scan the buyer's outgoing history for the explicit rejected row.
    bool sawRejected = false;
    if (outgoingAfterReject.has_value()) {
        for (const auto& row : *outgoingAfterReject) {
            if (row.offer_id == rejectOfferId && row.status == "Rejected") sawRejected = true;
        }
    }
    AcceptTest::checkTrue("9.13 Outgoing offers include rejected status",
                          sawRejected,
                          "Expected outgoing history to retain rejected offer row");
    currentSession.clear();
    AcceptTest::checkContains("9.14 Unauthenticated accept -> Session Expired",
                              "Session Expired",
                              Offer::accept(rejectOfferId));
    AcceptTest::checkContains("9.15 Unauthenticated reject -> Session Expired",
                              "Session Expired",
                              Offer::reject(rejectOfferId));
    printf("\n");

    // Release the in-memory DB before returning the aggregated suite result.
    AcceptTest::closeDb();
    return AcceptTest::finishSuite();
}
