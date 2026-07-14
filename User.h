// User.h
// MustangMarketplace — Group 49
// Stories 1 (Register) and 2 (Login)

#ifndef USER_H
#define USER_H

#include <string>
#include "Database.h"

/**
 * @file User.h
 * @brief User model, auth operations, and process-local session state.
 *
 * Declares the user domain object together with the legacy in-process session
 * snapshot used by business logic that still depends on `currentSession`.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Ashwin Subash (asubash2)
 */

// Holds the logged-in user's identity in memory for the session
/**
 * @struct Session
 * @brief Process-local authenticated session snapshot.
 *
 * Stores the minimal authenticated identity needed by legacy service methods
 * so they can act on behalf of the currently logged-in user.
 * @author Ashwin Subash (asubash2)
 */
struct Session {
    /** True when the process-local session currently represents a logged-in user. */
    bool active  = false;
    /** Authenticated user id stored in the legacy session snapshot. */
    int user_id = -1;
    /** Authenticated email stored in the legacy session snapshot. */
    std::string email;

    /** @brief Default empty session constructor.
     *
     * Initializes the session in an unauthenticated state with no user id and
     * no cached email address.
     */
    Session() = default;
    /** @brief Value constructor for an initialized session state.
     *
     * Creates a session snapshot with explicit authentication state and user
     * identity values.
     * @param a True when the session should start as authenticated.
     * @param id User id associated with the session.
     * @param e Email associated with the session.
     */
    Session(bool a, int id, const std::string& e) : active(a), user_id(id), email(e) {}

    /** @brief Resets session to unauthenticated state.
     *
     * Clears every cached identity field so later service calls treat the
     * process-local session as logged out.
     *  @author Ashwin Subash (asubash2)
     */
    void clear() {
        active  = false;
        user_id = -1;
        email.clear();
    }
};

/** @brief Global current session used by legacy service classes.
 *
 * Exposes the process-wide authenticated user snapshot consumed by older
 * domain methods that are not yet fully token-aware.
 */
extern Session currentSession;

/**
 * @class User
 * @brief User entity and account/authentication operations.
 *
 * Represents a user record snapshot and provides the registration and login
 * flows that populate the shared session state on success.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Ashwin Subash (asubash2)
 */
class User {
private:
    /** Persisted user id for this snapshot. */
    int user_id;
    /** Account email for this snapshot. */
    std::string email;
    /** Moderation flag indicating whether the account is banned. */
    bool is_banned;

public:
    /**
     * @brief Constructs a user object snapshot.
     *
     * Stores the supplied database fields in an in-memory object that can be
     * returned or inspected without further database access.
     * @param id Persisted user id.
     * @param email Account email for the user.
     * @param banned True when the account is currently banned.
     */
    User(int id, const std::string& email, bool banned);

    /** @brief Returns user id.
     *
     * Provides the persisted identifier associated with this user snapshot.
     * @return User id value.
     */
    int getUserId() const;
    /** @brief Returns user email.
     *
     * Provides the email address stored on this user snapshot.
     * @return User email string.
     */
    std::string getEmail()    const;
    /** @brief Returns user banned status.
     *
     * Indicates whether the user snapshot represents a banned account.
     * @return True when the user is banned.
     */
    bool getIsBanned() const;

    /**
     * @brief Returns SHA-256 hash of plaintext password (hex).
     *
     * Converts a plaintext password into the hexadecimal digest format stored
     * and compared by the authentication layer.
     * @author Ashwin Subash (asubash2)
     * @param password Plain password.
     * @return Hex-encoded digest.
     */
    static std::string hashPassword(const std::string& password);

    // Story 1: Register
    // PASS: valid @uwo.ca email -> "Registration successful", session created
    // FAIL: non-UWO email       -> "Invalid Domain"
    // FAIL: duplicate email     -> "Email Already Registered"
    /**
     * @brief Registers a new account and starts session on success.
     *
     * Validates the submitted account data, inserts a new user record, and
     * updates the shared process-local session when registration succeeds.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @param email New account email.
     * @param password Plain password.
     * @return Operation status string.
     */
    static std::string registerUser(const std::string& email, const std::string& password);

    // Story 2: Login
    // PASS: correct credentials -> "Login successful", session created
    // FAIL: wrong password       -> "Authentication Failed"
    // FAIL: email not in DB      -> "User Not Found"
    /**
     * @brief Authenticates user and starts session on success.
     *
     * Verifies the submitted credentials against stored account data and
     * populates the shared session state when the login succeeds.
     * @author Ashwin Subash (asubash2)
     * @param email Account email.
     * @param password Plain password.
     * @return Operation status string.
     */
    static std::string login(const std::string& email, const std::string& password);
};

#endif
