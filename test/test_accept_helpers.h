#ifndef TEST_ACCEPT_HELPERS_H
#define TEST_ACCEPT_HELPERS_H

#include <string>

#include <sqlite3.h>

/**
 * @file test_accept_helpers.h
 * @brief Shared acceptance-test harness declarations.
 *
 * Declares the reusable helpers that set up the in-memory SQLite fixture,
 * print suite output, and perform lightweight assertions for the grouped
 * acceptance-test binaries.
 */

/**
 * @brief Shared in-memory SQLite handle used by acceptance tests.
 *
 * Exposes the active test database connection so acceptance suites can read
 * generated ids or inspect DB state directly when needed.
 */
extern sqlite3* testDB;

/**
 * @brief Shared helpers for grouped acceptance-test binaries.
 *
 * Groups the small harness utilities used to bootstrap fixtures, print test
 * sections, execute assertions, and inspect database state across suites.
 */
namespace AcceptTest {

/**
 * @brief Starts a new acceptance suite and resets pass/fail counters.
 *
 * Prints the suite title banner and clears any previously accumulated result
 * counts so the current binary reports only its own outcomes.
 * @param title Human-readable suite title.
 */
void beginSuite(const std::string& title);
/**
 * @brief Prints a formatted story section heading.
 *
 * Adds a consistent visual separator before the assertions belonging to one
 * story or grouped story section.
 * @param label Story label to display.
 */
void printStoryHeader(const std::string& label);

/**
 * @brief Opens a fresh in-memory SQLite database for one suite run.
 *
 * Closes any existing acceptance-test database, opens a new isolated in-memory
 * database, and enables foreign-key checks.
 */
void openFreshDb();
/**
 * @brief Closes the current acceptance-test database.
 *
 * Releases the active in-memory SQLite handle and clears the exported pointer
 * so later suites start from a known empty state.
 */
void closeDb();
/**
 * @brief Seeds the shared fixture schema and baseline rows.
 *
 * Creates the common tables and inserts the users, listings, and photos needed
 * by most grouped acceptance suites.
 * @param existingPhotoPath Path to a guaranteed-existing file for photo tests.
 */
void seedBaseData(const std::string& existingPhotoPath);

/**
 * @brief Clears the legacy global session before a test case.
 *
 * Resets `currentSession` so each assertion can start from a known auth state.
 */
void resetSession();

/**
 * @brief Asserts that the actual string contains the expected substring.
 *
 * Prints a pass/fail line and updates suite counters using substring matching
 * so status messages can include additional context without breaking tests.
 * @param name Human-readable assertion label.
 * @param expected Substring expected to appear in the actual result.
 * @param got Actual result string under test.
 */
void checkContains(const std::string& name, const std::string& expected, const std::string& got);
/**
 * @brief Asserts that two strings are exactly equal.
 *
 * Prints a pass/fail line and updates suite counters using strict string
 * equality for cases where the output must be exact.
 * @param name Human-readable assertion label.
 * @param expected Expected full string.
 * @param got Actual full string.
 */
void checkEqual(const std::string& name, const std::string& expected, const std::string& got);
/**
 * @brief Asserts that a boolean condition is true.
 *
 * Prints a pass/fail line and optionally emits extra detail when the condition
 * fails.
 * @param name Human-readable assertion label.
 * @param condition Boolean condition to validate.
 * @param detail Optional extra diagnostic text for failures.
 */
void checkTrue(const std::string& name, bool condition, const std::string& detail = "");

/**
 * @brief Executes raw SQL against the shared test database.
 *
 * Runs setup or inspection SQL that is easier to express directly than through
 * domain methods.
 * @param sql SQL statement text to execute.
 * @return True when the statement succeeds.
 */
bool execSql(const std::string& sql);
/**
 * @brief Reads the first integer column from a scalar SQL query.
 *
 * Executes a query expected to return one row with one integer value and
 * returns `0` on preparation or stepping failure.
 * @param sql Scalar SQL query.
 * @return First integer column from the first row, or `0` on failure.
 */
int scalarInt(const std::string& sql);
/**
 * @brief Reads the first text column from a scalar SQL query.
 *
 * Executes a query expected to return one row with one text value and returns
 * an empty string on preparation or stepping failure.
 * @param sql Scalar SQL query.
 * @return First text column from the first row, or empty string on failure.
 */
std::string scalarText(const std::string& sql);
/**
 * @brief Extracts the title column from a recommendation TSV row.
 *
 * Pulls the recommendation title out of the shared listing TSV format so
 * recommendation assertions can compare only the title field.
 * @param payload Recommendation TSV row or sentinel status text.
 * @return Extracted title, or the original sentinel text.
 */
std::string recommendationTitleFromTsv(const std::string& payload);

/**
 * @brief Prints suite totals and returns the process exit code.
 *
 * Writes the final pass/fail summary and returns `0` when all assertions pass,
 * otherwise `1`.
 * @return Process exit code for the suite.
 */
int finishSuite();

}  // namespace AcceptTest

#endif
