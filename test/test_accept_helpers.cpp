/**
 * @file test_accept_helpers.cpp
 * @brief Shared acceptance-test harness implementation.
 *
 * Implements the common SQLite fixture setup, assertion helpers, and tiny DB
 * inspection utilities reused across the grouped acceptance suites.
 */

#include "test_accept_helpers.h"

#include "User.h"

#include <cstdio>
#include <string>

using namespace std;

sqlite3* testDB = nullptr;

namespace AcceptTest {

/** Number of successful assertions in the current suite. */
static int g_passed = 0;
/** Number of failed assertions in the current suite. */
static int g_failed = 0;

/**
 * @brief Starts a suite banner and resets counters.
 *
 * Clears the running counters so one test binary cannot leak results into the
 * next, then prints the suite header shown in CI logs.
 * @param title Human-readable suite title.
 */
void beginSuite(const string& title) {
    g_passed = 0;
    g_failed = 0;
    // Print a clean suite banner so CI logs are easy to scan by binary.
    printf("%s\n", title.c_str());
    printf("--------------------------------------------\n\n");
}

/**
 * @brief Prints a story heading inside the current suite.
 *
 * Separates the assertions for one story group from the next using a
 * consistent visual header.
 * @param label Story section label.
 */
void printStoryHeader(const string& label) {
    printf("── %s ──\n", label.c_str());
}

/**
 * @brief Opens a new in-memory database for the current suite.
 *
 * Ensures every grouped suite starts with a fresh isolated SQLite database and
 * foreign-key enforcement enabled.
 */
void openFreshDb() {
    // Always close any previous handle first so suites stay isolated.
    closeDb();
    sqlite3_open(":memory:", &testDB);
    // Mirror the production DB setting that enables FK validation.
    sqlite3_exec(testDB, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
}

/**
 * @brief Closes the active in-memory database.
 *
 * Releases the shared SQLite handle when a suite finishes or is being reset.
 */
void closeDb() {
    if (testDB) {
        sqlite3_close(testDB);
        testDB = nullptr;
    }
}

/**
 * @brief Creates the tables required by the acceptance suites.
 *
 * Builds the minimal schema shared by the grouped acceptance-test binaries so
 * each suite can start from a known database structure.
 */
static void createSchema() {
    // Create the same user table the domain layer expects in production.
    sqlite3_exec(testDB,
        "CREATE TABLE users ("
        "  user_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  email TEXT NOT NULL UNIQUE,"
        "  password_hash TEXT NOT NULL,"
        "  is_banned INTEGER DEFAULT 0"
        ");", nullptr, nullptr, nullptr);

    // Listings back most stories, including offers, search, purchases, and ratings.
    sqlite3_exec(testDB,
        "CREATE TABLE listings ("
        "  listing_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  seller_id INTEGER NOT NULL,"
        "  buyer_id INTEGER,"
        "  title TEXT NOT NULL,"
        "  description TEXT NOT NULL,"
        "  price REAL NOT NULL,"
        "  category TEXT,"
        "  status TEXT DEFAULT 'active'"
        ");", nullptr, nullptr, nullptr);

    // Watchlist history drives both watchlist and recommendation scenarios.
    sqlite3_exec(testDB,
        "CREATE TABLE watchlist ("
        "  watchlist_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  user_id INTEGER NOT NULL,"
        "  listing_id INTEGER NOT NULL,"
        "  UNIQUE(user_id, listing_id)"
        ");", nullptr, nullptr, nullptr);

    // Photos store one path per listing for the photo-view acceptance checks.
    sqlite3_exec(testDB,
        "CREATE TABLE photos ("
        "  photo_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  listing_id INTEGER NOT NULL,"
        "  file_path TEXT NOT NULL"
        ");", nullptr, nullptr, nullptr);

    // Ratings are keyed per listing so one completed purchase can be rated once.
    sqlite3_exec(testDB,
        "CREATE TABLE ratings ("
        "  rating_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  listing_id INTEGER NOT NULL,"
        "  buyer_id INTEGER NOT NULL,"
        "  seller_id INTEGER NOT NULL,"
        "  rating INTEGER NOT NULL,"
        "  comment TEXT,"
        "  UNIQUE(listing_id)"
        ");", nullptr, nullptr, nullptr);

    // Offers model the buyer/seller negotiation workflow under test.
    sqlite3_exec(testDB,
        "CREATE TABLE offers ("
        "  offer_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  listing_id INTEGER NOT NULL,"
        "  buyer_id INTEGER NOT NULL,"
        "  seller_id INTEGER NOT NULL,"
        "  offer_price REAL NOT NULL,"
        "  status TEXT DEFAULT 'Pending'"
        ");", nullptr, nullptr, nullptr);

    // Notifications capture offer-side effects that acceptance tests verify.
    sqlite3_exec(testDB,
        "CREATE TABLE notifications ("
        "  notification_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  user_id INTEGER NOT NULL,"
        "  message TEXT NOT NULL"
        ");", nullptr, nullptr, nullptr);
}

/**
 * @brief Seeds the common users, listings, photos, and sold fixture rows.
 *
 * Inserts the baseline data reused across most suites so each binary can focus
 * on the story-specific setup it needs beyond this shared foundation.
 * @param existingPhotoPath Path to a file guaranteed to exist for positive photo checks.
 */
void seedBaseData(const string& existingPhotoPath) {
    // Create the shared schema before inserting baseline rows.
    createSchema();

    // Baseline buyer/seller account used by most auth and listing flows.
    string h1 = User::hashPassword("correct123");
    sqlite3_exec(testDB,
        ("INSERT INTO users (email, password_hash, is_banned) VALUES "
         "('test@uwo.ca', '" + h1 + "', 0);").c_str(),
        nullptr, nullptr, nullptr);

    // Alternate user used for ownership, moderation, and cross-user scenarios.
    string h2 = User::hashPassword("pass2");
    // Baseline seller-owned listing used by delete and photo tests.
    sqlite3_exec(testDB,
        ("INSERT INTO users (email, password_hash, is_banned) VALUES "
         "('other@uwo.ca', '" + h2 + "', 0);").c_str(),
        nullptr, nullptr, nullptr);

    // Active books listing used by search, watchlist, and recommendation tests.
    sqlite3_exec(testDB,
        "INSERT INTO listings (seller_id, title, description, price, status) "
        "VALUES (1, 'Calculus Textbook', '9th edition, good condition', 40.0, 'active');",
        nullptr, nullptr, nullptr);

    // Active electronics listing used by search-filter assertions.
    sqlite3_exec(testDB,
        "INSERT INTO listings (seller_id, title, description, price, category, status) "
        "VALUES (2, 'Physics Notes', 'Great summary sheets', 15.0, 'Books', 'active');",
        nullptr, nullptr, nullptr);

    // Seller-1 books listing used as part of watchlist category history cases.
    sqlite3_exec(testDB,
        "INSERT INTO listings (seller_id, title, description, price, category, status) "
        "VALUES (2, 'Wireless Mouse', 'Bluetooth mouse', 25.0, 'Electronics', 'active');",
        nullptr, nullptr, nullptr);

    // Positive photo case points at a real file path supplied by the running test binary.
    sqlite3_exec(testDB,
        "INSERT INTO listings (seller_id, title, description, price, category, status) "
        "VALUES (1, 'Old Chemistry Book', 'Used but fine', 10.0, 'Books', 'active');",
        nullptr, nullptr, nullptr);

    // Negative photo case points at a path that should not exist.
    sqlite3_exec(testDB,
        ("INSERT INTO photos (listing_id, file_path) VALUES (1, '" + existingPhotoPath + "');").c_str(),
        nullptr, nullptr, nullptr);

    // Shared sold listing supports rating and purchase-oriented assertions.
    sqlite3_exec(testDB,
        "INSERT INTO photos (listing_id, file_path) VALUES (2, '/tmp/nonexistent_photo.jpg');",
        nullptr, nullptr, nullptr);

    sqlite3_exec(testDB,
        "INSERT INTO listings (seller_id, title, description, price, status) "
        "VALUES (2, 'Sold Item', 'sold desc', 50.0, 'sold');",
        nullptr, nullptr, nullptr);
}

/**
 * @brief Resets the legacy global session state.
 *
 * Clears the shared `currentSession` snapshot so later assertions start from a
 * predictable authentication state.
 */
void resetSession() {
    currentSession.clear();
}

/**
 * @brief Performs a substring-based acceptance assertion.
 *
 * Treats the assertion as passing when the expected text appears anywhere in
 * the actual result string.
 * @param name Human-readable assertion label.
 * @param expected Expected substring.
 * @param got Actual result string.
 */
void checkContains(const string& name, const string& expected, const string& got) {
    bool ok = (got.find(expected) != string::npos);
    // Emit a stable one-line result for easy CI scanning.
    printf("%s %s\n", ok ? "[PASS]" : "[FAIL]", name.c_str());
    if (!ok) {
        printf("       Expected substring: \"%s\"  Got: \"%s\"\n", expected.c_str(), got.c_str());
    }
    // Update the final suite summary counters.
    if (ok) ++g_passed;
    else ++g_failed;
}

/**
 * @brief Performs an exact-string acceptance assertion.
 *
 * Treats the assertion as passing only when the expected and actual strings
 * match exactly.
 * @param name Human-readable assertion label.
 * @param expected Expected full string.
 * @param got Actual full string.
 */
void checkEqual(const string& name, const string& expected, const string& got) {
    bool ok = (expected == got);
    printf("%s %s\n", ok ? "[PASS]" : "[FAIL]", name.c_str());
    if (!ok) {
        printf("       Expected: \"%s\"  Got: \"%s\"\n", expected.c_str(), got.c_str());
    }
    // Update the final suite summary counters.
    if (ok) ++g_passed;
    else ++g_failed;
}

/**
 * @brief Performs a boolean acceptance assertion.
 *
 * Treats the assertion as passing when the supplied boolean condition is true
 * and optionally prints extra detail when it fails.
 * @param name Human-readable assertion label.
 * @param condition Boolean predicate under test.
 * @param detail Optional extra failure detail.
 */
void checkTrue(const string& name, bool condition, const string& detail) {
    printf("%s %s\n", condition ? "[PASS]" : "[FAIL]", name.c_str());
    if (!condition && !detail.empty()) {
        printf("       %s\n", detail.c_str());
    }
    // Update the final suite summary counters.
    if (condition) ++g_passed;
    else ++g_failed;
}

/**
 * @brief Executes raw SQL and reports setup failures as test failures.
 *
 * Lets suites express direct fixture mutations while still surfacing SQL
 * issues in the same pass/fail output stream.
 * @param sql SQL text to execute against `testDB`.
 * @return True when the SQL succeeds.
 */
bool execSql(const string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(testDB, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        // Promote SQL setup failures into visible suite failures.
        string msg = err ? err : "unknown sqlite error";
        sqlite3_free(err);
        printf("[FAIL] SQL setup failed: %s\n", msg.c_str());
        ++g_failed;
        return false;
    }
    return true;
}

/**
 * @brief Returns the integer result of a scalar SQL query.
 *
 * Executes a one-row/one-column integer query used by assertions that inspect
 * resulting database state.
 * @param sql Scalar SQL query.
 * @return Integer value from the first row, or `0` when the query cannot run.
 */
int scalarInt(const string& sql) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(testDB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    int value = 0;
    // Use the first column of the first row as the scalar assertion value.
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        value = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return value;
}

/**
 * @brief Returns the text result of a scalar SQL query.
 *
 * Executes a one-row/one-column text query used by assertions that inspect DB
 * state after domain actions run.
 * @param sql Scalar SQL query.
 * @return Text value from the first row, or empty string on failure.
 */
string scalarText(const string& sql) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(testDB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return "";
    }
    string value;
    // Use the first column of the first row as the scalar assertion value.
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        value = text ? reinterpret_cast<const char*>(text) : "";
    }
    sqlite3_finalize(stmt);
    return value;
}

/**
 * @brief Extracts the title field from a recommendation TSV payload.
 *
 * Converts the shared listing TSV wire format into a simpler title string for
 * recommendation-specific assertions.
 * @param payload Recommendation TSV row or sentinel status string.
 * @return Extracted title, or the original sentinel string.
 */
string recommendationTitleFromTsv(const string& payload) {
    // Preserve sentinel values so callers can assert on empty-market states too.
    if (payload == "MARKET_EMPTY" || payload == "DB_ERROR" || payload.empty()) return payload;
    size_t firstTab = payload.find('\t');
    if (firstTab == string::npos) return payload;
    size_t secondTab = payload.find('\t', firstTab + 1);
    if (secondTab == string::npos) return payload.substr(firstTab + 1);
    // The title is the second column in the shared listing TSV format.
    return payload.substr(firstTab + 1, secondTab - firstTab - 1);
}

/**
 * @brief Prints the suite summary and returns the final exit code.
 *
 * Writes the final totals used in CI logs and converts the aggregate result
 * into a standard success/failure process exit code.
 * @return `0` when all assertions passed, otherwise `1`.
 */
int finishSuite() {
    printf("\n--------------------------------------------\n");
    printf("Results: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}

}  // namespace AcceptTest
