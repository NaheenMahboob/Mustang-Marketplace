/**
 * @file test_accept_listings.cpp
 * @brief Acceptance tests for listing creation, deletion, search, editing, tagging, and photo view.
 *
 * Runs grouped acceptance scenarios for Stories 5, 6, 7, 10, 11, 14, and 16
 * using the shared in-memory acceptance-test harness.
 */

#include "test_accept_helpers.h"

#include "Listing.h"
#include "Photo.h"
#include "User.h"

#include <filesystem>
#include <set>
#include <string>

using namespace std;

/**
 * @brief Returns a tiny PNG-signature payload for raw photo upload tests.
 *
 * The raw photo path only needs stable bytes with a PNG signature so
 * `Photo::saveRaw` chooses a `.png` path and `Photo::loadRaw` can verify the
 * stored bytes round-trip correctly.
 * @return Small deterministic PNG-like byte payload.
 */
static string makePngLikePayloadA() {
    return string("\x89PNG\r\n\x1a\nraw-photo-a", 19);
}

/**
 * @brief Returns a second tiny PNG-signature payload for replacement tests.
 *
 * Uses a different trailing byte sequence so the suite can confirm that a
 * later raw upload replaces the earlier stored image bytes.
 * @return Small deterministic PNG-like byte payload distinct from payload A.
 */
static string makePngLikePayloadB() {
    return string("\x89PNG\r\n\x1a\nraw-photo-b", 19);
}

/**
 * @brief Captures the current set of files under the repo-local uploads directory.
 *
 * Lets the raw-photo acceptance tests remove only the files they created during
 * the current run without disturbing any pre-existing upload artifacts.
 * @return Absolute paths for the upload files currently present.
 */
static set<string> snapshotUploadFiles() {
    set<string> files;
    const filesystem::path uploadsDir = filesystem::current_path() / "uploads";
    if (!filesystem::exists(uploadsDir)) {
        return files;
    }
    for (const auto& entry : filesystem::directory_iterator(uploadsDir)) {
        if (entry.is_regular_file()) {
            files.insert(entry.path().string());
        }
    }
    return files;
}

/**
 * @brief Removes upload files that were created after an earlier snapshot.
 *
 * Compares the current contents of `uploads/` against a pre-suite snapshot and
 * deletes only the newly created files, leaving any pre-existing artifacts
 * untouched.
 * @param before Absolute paths present before the suite began creating uploads.
 */
static void cleanupNewUploadFiles(const set<string>& before) {
    const set<string> after = snapshotUploadFiles();
    for (const string& path : after) {
        if (before.find(path) == before.end()) {
            filesystem::remove(path);
        }
    }
}

/**
 * @brief Executes the listings/search/photo acceptance suite.
 *
 * Seeds a fresh in-memory fixture, performs listing-related story actions, and
 * verifies both domain return values and selected database side effects.
 * @param argc Number of command-line arguments.
 * @param argv Command-line arguments, used to harvest an existing file path for photo fixtures.
 * @return Process exit code for the suite.
 */
int main(int argc, char** argv) {
    // Reuse the running binary path as a guaranteed-existing file fixture for positive photo tests.
    const string photoFixture = (argc > 0) ? argv[0] : "./test_accept_listings_bin";
    const set<string> uploadsBeforeSuite = snapshotUploadFiles();

    // Prepare a clean database and baseline rows before the grouped listing assertions begin.
    AcceptTest::openFreshDb();
    AcceptTest::seedBaseData(photoFixture);
    Photo::setAssetRoot(std::filesystem::current_path().string());
    AcceptTest::beginSuite("MustangMarketplace — Acceptance Tests (Listings / Search / Photos)");

    AcceptTest::printStoryHeader("Story 5: Create Listing");
    // Authenticate as seller 1 before exercising the listing-creation path.
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("5.1 Valid listing -> Listing created",
                              "Listing created",
                              Listing::create("Desk Lamp", "LED lamp", 18.0, "Home"));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("5.2 Missing description -> Invalid input",
                              "Invalid input",
                              Listing::create("Chair", "", 12.0, "Furniture"));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("5.3 Negative price -> Invalid input",
                              "Invalid input",
                              Listing::create("Notebook", "Spiral notebook", -5.0, "School"));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("5.4 Zero price -> Listing created",
                              "Listing created",
                              Listing::create("Free Notebook", "still valid", 0.0, "School"));
    // Flip the moderation flag to verify banned sellers cannot create listings.
    AcceptTest::execSql("UPDATE users SET is_banned = 1 WHERE user_id = 1;");
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("5.5 Banned seller -> Access Denied",
                              "Access Denied",
                              Listing::create("Blocked Listing", "ban check", 5.0, "Books"));
    // Restore the seller for the later story sections in this suite.
    AcceptTest::execSql("UPDATE users SET is_banned = 0 WHERE user_id = 1;");
    printf("\n");

    AcceptTest::printStoryHeader("Story 6: Delete Listing");
    // Delete the seeded seller-owned listing, then verify cross-user and missing-item branches.
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("6.1 Owner deletes listing -> Listing removed",
                              "Listing removed",
                              Listing::remove(1));
    // Repeating the delete against the same listing id should now hit the unavailable-item branch.
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("6.2 Repeated delete on same listing -> Item Not Found",
                              "Item Not Found",
                              Listing::remove(1));
    // Insert a fresh seller-owned listing so the non-owner case can target a real active row.
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, status) "
        "VALUES (1, 'Another Book', 'desc', 20.0, 'active');");
    int listing2 = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    currentSession = {true, 2, "other@uwo.ca"};
    AcceptTest::checkContains("6.3 Non-owner -> Access Denied",
                              "Access Denied",
                              Listing::remove(listing2));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("6.4 Missing listing -> Item Not Found",
                              "Item Not Found",
                              Listing::remove(9999));
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, buyer_id, title, description, price, status) "
        "VALUES (1, 2, 'Sold Delete Item', 'desc', 19.0, 'sold');");
    int soldDeleteListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("6.5 Sold listing delete -> Item Not Found",
                              "Item Not Found",
                              Listing::remove(soldDeleteListingId));
    currentSession.clear();
    AcceptTest::checkContains("6.6 Unauthenticated delete -> Session Expired",
                              "Session Expired",
                              Listing::remove(listing2));
    printf("\n");

    AcceptTest::printStoryHeader("Story 7 and 11: Search / Filtering");
    // Add extra active listings so search/filter assertions can inspect keyword, category, and sort behavior.
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, category, status) "
        "VALUES (2, 'Algorithms Textbook', 'search test textbook', 30.0, 'Textbook', 'active');");
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, category, status) "
        "VALUES (2, 'Data Structures Textbook', 'cheaper search test textbook', 12.0, 'Textbook', 'active');");
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, category, status) "
        "VALUES (2, 'Newest Browse Listing', 'browse me', 44.0, 'Sports', 'active');");
    AcceptTest::checkContains("7.1 Keyword matches listing -> results displayed",
                              "Physics Notes",
                              Listing::search("notes"));
    AcceptTest::checkContains("7.2 Blank keyword browse -> active listings shown",
                              "Newest Browse Listing",
                              Listing::search(""));
    AcceptTest::checkContains("11.1 Category filter narrows results",
                              "Wireless Mouse",
                              Listing::search("", "Electronics", "none"));
    AcceptTest::checkContains("11.2 Category + keyword works together",
                              "Algorithms Textbook",
                              Listing::search("Algorithms", "Textbook", "none"));
    AcceptTest::checkContains("11.3 Low to High price ordering works",
                              "Data Structures Textbook",
                              Listing::search("", "Textbook", "asc"));
    AcceptTest::checkContains("11.4 High to Low price ordering works",
                              "Algorithms Textbook",
                              Listing::search("", "Textbook", "desc"));
    AcceptTest::checkContains("7.3 Keyword with no matches -> No Results Found",
                              "No Results Found",
                              Listing::search("xyznothing"));
    AcceptTest::checkContains("11.5 Category with no matches -> No Results Found",
                              "No Results Found",
                              Listing::search("", "Clothing", "none"));
    printf("\n");

    AcceptTest::printStoryHeader("Story 10: Tag Listing");
    // Insert a fresh active listing so tag assertions do not depend on earlier story mutations.
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, status) "
        "VALUES (1, 'Tag Test Book', 'desc', 10.0, 'active');");
    int tagListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("10.1 Valid category -> Category assigned",
                              "Category assigned",
                              Listing::tag(tagListingId, "Textbook"));
    AcceptTest::checkContains("10.2 Empty category -> Selection Required",
                              "Selection Required",
                              Listing::tag(tagListingId, ""));
    AcceptTest::checkContains("10.3 Invalid category -> Selection Required",
                              "Selection Required",
                              Listing::tag(tagListingId, "Miscellaneous"));
    currentSession = {true, 2, "other@uwo.ca"};
    AcceptTest::checkContains("10.4 Non-owner -> Access Denied",
                              "Access Denied",
                              Listing::tag(tagListingId, "Textbook"));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("10.5 Missing item -> Item Not Found",
                              "Item Not Found",
                              Listing::tag(9999, "Textbook"));
    currentSession.clear();
    AcceptTest::checkContains("10.6 Unauthenticated tag -> Session Expired",
                              "Session Expired",
                              Listing::tag(tagListingId, "Textbook"));
    printf("\n");

    AcceptTest::printStoryHeader("Story 14: Edit Listing");
    // Insert a separate listing dedicated to edit validations and ownership checks.
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, status) "
        "VALUES (1, 'Edit Test Item', 'original desc', 30.0, 'active');");
    int editListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("14.1 Valid edit -> Listing updated",
                              "Listing updated",
                              Listing::edit(editListingId, "Updated Title", "Updated desc", 35.0));
    currentSession = {true, 2, "other@uwo.ca"};
    AcceptTest::checkContains("14.2 Non-owner -> Access Denied",
                              "Access Denied",
                              Listing::edit(editListingId, "Hacked Title", "hacked desc", 1.0));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("14.3 Empty title -> Invalid input",
                              "Invalid input",
                              Listing::edit(editListingId, "", "some desc", 10.0));
    AcceptTest::checkContains("14.4 Negative price -> Invalid input",
                              "Invalid input",
                              Listing::edit(editListingId, "Title", "some desc", -5.0));
    AcceptTest::checkContains("14.5 Missing item -> Item Not Found",
                              "Item Not Found",
                              Listing::edit(9999, "Title", "some desc", 10.0));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("14.6 Zero price edit -> Listing updated",
                              "Listing updated",
                              Listing::edit(editListingId, "Zero Price Title", "free item", 0.0));
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, buyer_id, title, description, price, status) "
        "VALUES (1, 2, 'Sold Edit Item', 'desc', 15.0, 'sold');");
    int soldEditListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("14.7 Sold listing edit -> Item Not Found",
                              "Item Not Found",
                              Listing::edit(soldEditListingId, "Updated", "desc", 10.0));
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, status) "
        "VALUES (1, 'Deleted Edit Item', 'desc', 15.0, 'deleted');");
    int deletedEditListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("14.8 Deleted listing edit -> Item Not Found",
                              "Item Not Found",
                              Listing::edit(deletedEditListingId, "Updated", "desc", 10.0));
    currentSession.clear();
    AcceptTest::checkContains("14.9 Unauthenticated edit -> Session Expired",
                              "Session Expired",
                              Listing::edit(editListingId, "Updated", "desc", 10.0));
    printf("\n");

    AcceptTest::printStoryHeader("Story 16: Load Listing Photo");
    // Reuse the seeded positive, broken, and missing photo rows to cover the legacy seeded load branches.
    string loadedBytes;
    string contentType;
    AcceptTest::checkTrue("16.1 Valid file path -> raw photo loads",
                          Photo::loadRaw(1, loadedBytes, contentType),
                          "expected seeded photo row to load raw bytes successfully");
    AcceptTest::checkTrue("16.2 Valid file path -> raw bytes are non-empty",
                          !loadedBytes.empty(),
                          "expected seeded photo file to produce non-empty bytes");
    AcceptTest::checkEqual("16.3 Seeded fixture content type -> application/octet-stream",
                           "application/octet-stream",
                           contentType);

    loadedBytes = "sentinel";
    contentType = "sentinel/type";
    AcceptTest::checkTrue("16.4 Broken file path -> raw load returns false",
                          !Photo::loadRaw(2, loadedBytes, contentType),
                          "expected broken stored path to fail raw load");
    AcceptTest::checkEqual("16.5 Broken file path clears loaded bytes",
                           "",
                           loadedBytes);
    AcceptTest::checkEqual("16.6 Broken file path clears content type",
                           "",
                           contentType);

    loadedBytes = "sentinel";
    contentType = "sentinel/type";
    AcceptTest::checkTrue("16.7 No photo entry -> raw load returns false",
                          !Photo::loadRaw(3, loadedBytes, contentType),
                          "expected missing photo row to fail raw load");
    AcceptTest::checkEqual("16.8 No photo entry clears loaded bytes",
                           "",
                           loadedBytes);
    AcceptTest::checkEqual("16.9 No photo entry clears content type",
                           "",
                           contentType);
    printf("\n");

    AcceptTest::printStoryHeader("Story 16B: Raw Photo Upload / Load");
    // Create a dedicated active seller-owned listing for raw upload/load assertions.
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, status) "
        "VALUES (1, 'Raw Photo Item', 'raw photo desc', 22.0, 'active');");
    int rawListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    const string pngPayloadA = makePngLikePayloadA();
    const string pngPayloadB = makePngLikePayloadB();
    loadedBytes.clear();
    contentType.clear();

    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("16B.1 Valid raw upload by owner -> Photo saved",
                              "Photo saved",
                              Photo::saveRaw(rawListingId, pngPayloadA));
    AcceptTest::checkTrue("16B.2 Valid raw load after upload -> returns true",
                          Photo::loadRaw(rawListingId, loadedBytes, contentType),
                          "expected raw photo load to succeed");
    AcceptTest::checkTrue("16B.3 Loaded bytes are non-empty",
                          !loadedBytes.empty(),
                          "expected uploaded raw bytes to be readable");
    AcceptTest::checkEqual("16B.4 Loaded bytes match uploaded bytes exactly",
                           pngPayloadA,
                           loadedBytes);
    AcceptTest::checkEqual("16B.5 Loaded content type is image/png",
                           "image/png",
                           contentType);

    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("16B.6 Re-upload replaces previous raw photo -> Photo saved",
                              "Photo saved",
                              Photo::saveRaw(rawListingId, pngPayloadB));
    loadedBytes.clear();
    contentType.clear();
    AcceptTest::checkTrue("16B.7 Replacement raw load succeeds",
                          Photo::loadRaw(rawListingId, loadedBytes, contentType),
                          "expected replacement raw photo load to succeed");
    AcceptTest::checkEqual("16B.8 Replacement bytes match latest upload",
                           pngPayloadB,
                           loadedBytes);
    AcceptTest::checkEqual("16B.9 Replacement content type stays image/png",
                           "image/png",
                           contentType);

    currentSession.clear();
    AcceptTest::checkContains("16B.10 Unauthenticated raw upload -> Session Expired",
                              "Session Expired",
                              Photo::saveRaw(rawListingId, pngPayloadA));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("16B.11 Empty raw upload -> Invalid image data",
                              "Invalid image data",
                              Photo::saveRaw(rawListingId, ""));
    currentSession = {true, 2, "other@uwo.ca"};
    AcceptTest::checkContains("16B.12 Non-owner raw upload -> Access Denied",
                              "Access Denied",
                              Photo::saveRaw(rawListingId, pngPayloadA));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("16B.13 Missing listing raw upload -> Item Not Found",
                              "Item Not Found",
                              Photo::saveRaw(9999, pngPayloadA));
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, buyer_id, title, description, price, status) "
        "VALUES (1, 2, 'Sold Raw Photo Item', 'desc', 18.0, 'sold');");
    int soldRawListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("16B.14 Sold listing raw upload -> Item Not Available",
                              "Item Not Available",
                              Photo::saveRaw(soldRawListingId, pngPayloadA));
    AcceptTest::execSql(
        "INSERT INTO listings (seller_id, title, description, price, status) "
        "VALUES (1, 'Deleted Raw Photo Item', 'desc', 18.0, 'deleted');");
    int deletedRawListingId = static_cast<int>(sqlite3_last_insert_rowid(testDB));
    currentSession = {true, 1, "test@uwo.ca"};
    AcceptTest::checkContains("16B.15 Deleted listing raw upload -> Item Not Available",
                              "Item Not Available",
                              Photo::saveRaw(deletedRawListingId, pngPayloadA));

    loadedBytes = "sentinel";
    contentType = "sentinel/type";
    AcceptTest::checkTrue("16B.16 Raw load with no photo row -> false",
                          !Photo::loadRaw(3, loadedBytes, contentType),
                          "expected raw load to fail when no photo row exists");
    AcceptTest::checkEqual("16B.17 No photo row clears loaded bytes",
                           "",
                           loadedBytes);
    AcceptTest::checkEqual("16B.18 No photo row clears content type",
                           "",
                           contentType);

    loadedBytes = "sentinel";
    contentType = "sentinel/type";
    AcceptTest::checkTrue("16B.19 Raw load with broken path row -> false",
                          !Photo::loadRaw(2, loadedBytes, contentType),
                          "expected raw load to fail for broken stored path");
    AcceptTest::checkEqual("16B.20 Broken path clears loaded bytes",
                           "",
                           loadedBytes);
    AcceptTest::checkEqual("16B.21 Broken path clears content type",
                           "",
                           contentType);
    printf("\n");

    // Release the in-memory DB before returning the aggregated suite result.
    AcceptTest::closeDb();
    cleanupNewUploadFiles(uploadsBeforeSuite);
    return AcceptTest::finishSuite();
}
