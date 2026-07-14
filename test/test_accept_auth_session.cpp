/**
 * @file test_accept_auth_session.cpp
 * @brief Acceptance tests for registration, login, session retention, and logout behavior.
 *
 * Runs grouped acceptance scenarios for Stories 1 through 4 using the shared
 * in-memory test harness.
 */

#include "test_accept_helpers.h"

#include "Listing.h"
#include "SessionBridge.h"
#include "User.h"

#include <string>

using namespace std;

/**
 * @brief Executes the auth/session acceptance suite.
 *
 * Seeds a fresh in-memory fixture, runs the registration and login scenarios,
 * then verifies session retention and session clearing behavior.
 * @param argc Number of command-line arguments.
 * @param argv Command-line arguments, used to harvest an existing file path for photo fixtures.
 * @return Process exit code for the suite.
 */
int main(int argc, char** argv) {
    // Reuse the running binary path as a guaranteed-existing file fixture if needed by shared seed data.
    const string photoFixture = (argc > 0) ? argv[0] : "./test_accept_auth_session_bin";

    // Start from a clean isolated database and insert the shared baseline rows.
    AcceptTest::openFreshDb();
    AcceptTest::seedBaseData(photoFixture);
    AcceptTest::beginSuite("MustangMarketplace — Acceptance Tests (Auth / Session)");

    AcceptTest::printStoryHeader("Story 1: Register");
    // Clear any leftover auth state before registration assertions.
    AcceptTest::resetSession();
    AcceptTest::checkContains("1.1 Valid @uwo.ca email -> Registration successful",
                              "Registration successful",
                              User::registerUser("newuser@uwo.ca", "password123"));
    AcceptTest::resetSession();
    AcceptTest::checkContains("1.2 Non-UWO email -> Invalid Domain",
                              "Invalid Domain",
                              User::registerUser("newuser@gmail.com", "password123"));
    AcceptTest::resetSession();
    AcceptTest::checkContains("1.3 Duplicate email -> Email Already Registered",
                              "Email Already Registered",
                              User::registerUser("test@uwo.ca", "password123"));
    printf("\n");

    AcceptTest::printStoryHeader("Story 2: Login");
    // Each login case starts from a logged-out baseline to avoid state leakage.
    AcceptTest::resetSession();
    AcceptTest::checkContains("2.1 Correct credentials -> Login successful",
                              "Login successful",
                              User::login("test@uwo.ca", "correct123"));
    AcceptTest::resetSession();
    AcceptTest::checkContains("2.2 Wrong password -> Authentication Failed",
                              "Authentication Failed",
                              User::login("test@uwo.ca", "wrongpassword"));
    AcceptTest::resetSession();
    AcceptTest::checkContains("2.3 Unknown email -> User Not Found",
                              "User Not Found",
                              User::login("ghost@uwo.ca", "anypass"));
    AcceptTest::resetSession();
    AcceptTest::checkContains("2.4 Empty password -> Authentication Failed",
                              "Authentication Failed",
                              User::login("test@uwo.ca", ""));
    // Temporarily ban the second user to verify moderation blocks login.
    AcceptTest::execSql("UPDATE users SET is_banned = 1 WHERE user_id = 2;");
    AcceptTest::resetSession();
    AcceptTest::checkContains("2.5 Banned user -> Access Denied",
                              "Access Denied",
                              User::login("other@uwo.ca", "pass2"));
    // Restore the user so later suites can reuse the shared baseline account.
    AcceptTest::execSql("UPDATE users SET is_banned = 0 WHERE user_id = 2;");
    AcceptTest::resetSession();
    AcceptTest::checkContains("2.6 First successful login establishes baseline session",
                              "Login successful",
                              User::login("test@uwo.ca", "correct123"));
    AcceptTest::checkContains("2.7 Second successful login replaces session identity",
                              "Login successful",
                              User::login("other@uwo.ca", "pass2"));
    AcceptTest::checkEqual("2.8 Replaced session stores second user id", "2", to_string(currentSession.user_id));
    AcceptTest::checkEqual("2.9 Replaced session stores second user email", "other@uwo.ca", currentSession.email);
    printf("\n");

    AcceptTest::printStoryHeader("Story 3: Retain Session In Memory");
    // Story 3 maps to the bridge helper that populates the legacy global session.
    AcceptTest::resetSession();
    syncSessionForNetwork(1, "test@uwo.ca");
    AcceptTest::checkTrue("3.1 syncSessionForNetwork activates session", currentSession.active);
    AcceptTest::checkEqual("3.2 syncSessionForNetwork stores user id", "1", to_string(currentSession.user_id));
    AcceptTest::checkEqual("3.3 syncSessionForNetwork stores email", "test@uwo.ca", currentSession.email);
    // Prove that a domain method can immediately use the bridged session.
    AcceptTest::checkContains("3.4 Retained session enables legacy domain call",
                              "Listing created",
                              Listing::create("Bridge Listing", "created from bridged session", 12.0, "Books"));
    printf("\n");

    AcceptTest::printStoryHeader("Story 4: Close Session On Logout");
    // Re-establish a live session, then clear it to simulate logout/session teardown.
    syncSessionForNetwork(1, "test@uwo.ca");
    currentSession.clear();
    AcceptTest::checkTrue("4.1 clear() closes in-memory session", !currentSession.active);
    AcceptTest::checkContains("4.2 Closed session blocks authenticated listing create",
                              "Session Expired",
                              Listing::create("Should Fail", "session cleared", 9.0, "Books"));
    printf("\n");

    // Release the in-memory DB before returning the aggregated suite result.
    AcceptTest::closeDb();
    return AcceptTest::finishSuite();
}
