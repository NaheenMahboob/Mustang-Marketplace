/**
 * @file User.cpp
 * @brief User model, auth operations, and process-local session state implementation.
 *
 * Implements the user domain object together with the legacy in-process
 * session snapshot used by business logic that still depends on
 * `currentSession`.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Ashwin Subash (asubash2)
 */

#include <string>
using namespace std;
// User.cpp
// MustangMarketplace — Group 49

#include "User.h"
#include <cstdio>
#include <openssl/sha.h>

Session currentSession;

// Constructor
User::User(int id, const string& email, bool banned)
    : user_id(id), email(email), is_banned(banned) {}

// Getters
int User::getUserId() const { return user_id; }
string User::getEmail()    const { return email; }
bool User::getIsBanned() const { return is_banned; }

string User::hashPassword(const string& password) {
    // SHA-256 hash used for storage/verification (no plaintext persistence).
    unsigned char hash[SHA256_DIGEST_LENGTH]; // raw 32-byte hash output
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()),
           password.size(), hash); // run SHA-256 on the password bytes
    char hex[SHA256_DIGEST_LENGTH * 2 + 1]; // 2 hex chars per byte + null terminator
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        snprintf(hex + (i * 2), 3, "%02x", hash[i]); // write each byte as 2-digit hex
    }
    // Return the full lowercase hexadecimal digest string.
    return string(hex);
}

string User::registerUser(const string& email, const string& password) {
    // Enforce uwo.ca domain requirement for registration.
    const string domain = "@uwo.ca";  // Required email suffix for valid marketplace accounts.
    if (email.size() <= domain.size() ||
        email.compare(email.size() - domain.size(), domain.size(), domain) != 0) {
        printf("Invalid Domain\n");
        return "Invalid Domain";
    }

    sqlite3* db = Database::open();  // Database handle used for duplicate checks and account creation.
    if (!db) { return "DB Error"; }

    // Reject duplicate emails before insert.
    const string checkSql = "SELECT user_id FROM users WHERE email = ?;";  // Query that detects existing accounts with the same email.
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the duplicate-email lookup.
    if (sqlite3_prepare_v2(db, checkSql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        Database::close(db);
        return "DB Error";
    }
    // Use the submitted email as the duplicate-check key.
    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_finalize(stmt);
        Database::close(db);
        printf("Email Already Registered\n");
        return "Email Already Registered";
    }
    sqlite3_finalize(stmt);

    // Insert account and initialize session state on success.
    const string insertSql = "INSERT INTO users (email, password_hash, is_banned) VALUES (?, ?, 0);";  // Insert used to create a new unbanned user row.
    sqlite3_stmt* insertStmt = nullptr;  // Prepared statement for the account insert.
    if (sqlite3_prepare_v2(db, insertSql.c_str(), -1, &insertStmt, nullptr) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        Database::close(db);
        return "DB Error";
    }

    // Hash the password before storing anything in the database.
    const string hashed = hashPassword(password);  // SHA-256 digest stored instead of the raw password.
    sqlite3_bind_text(insertStmt, 1, email.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insertStmt, 2, hashed.c_str(), -1, SQLITE_STATIC);

    string result;  // Final registration outcome returned to the caller.
    if (sqlite3_step(insertStmt) == SQLITE_DONE) {
        // Capture the generated id and immediately establish the new session.
        int newUserId = (int)sqlite3_last_insert_rowid(db);  // Freshly assigned user id for the newly created account.
        currentSession.active = true;
        currentSession.user_id = newUserId;
        currentSession.email = email;
        result = "Registration successful";
    } else if (sqlite3_extended_errcode(db) == SQLITE_CONSTRAINT_UNIQUE) {
        // Surface the UNIQUE constraint as the same duplicate-email message.
        result = "Email Already Registered";
    } else {
        result = "DB Error";
    }

    sqlite3_finalize(insertStmt);
    // Release the DB handle before returning the final registration outcome.
    Database::close(db);
    printf("%s\n", result.c_str());
    return result;
}

string User::login(const string& email, const string& password) {
    // Lookup credentials and enforce ban status before authentication success.
    sqlite3* db = Database::open();  // Database handle used for the credential lookup.
    if (!db) { return "DB Error"; }

    const string sql = "SELECT user_id, password_hash, is_banned FROM users WHERE email = ?;";  // Login query that fetches the stored hash and ban flag.
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the login lookup.
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        Database::close(db);
        return "DB Error";
    }
    // Bind the candidate email so SQLite checks only that account row.
    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

    string result;  // Final login outcome returned to the caller.
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int uid = sqlite3_column_int(stmt, 0);  // User id for the account matching the submitted email.
        string stored_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));  // Password hash loaded from the database.
        int is_banned = sqlite3_column_int(stmt, 2);  // Moderation flag that blocks banned users from logging in.

        if (is_banned) {
            // Banned accounts are denied before password verification can succeed.
            result = "Access Denied";
        } else if (hashPassword(password) == stored_hash) {
            // Matching password hash means the user is authenticated.
            currentSession.active = true;
            currentSession.user_id = uid;
            currentSession.email = email;
            result = "Login successful";
        } else {
            // The account exists, but the password check failed.
            result = "Authentication Failed";
        }
    } else {
        // No row means the submitted email is not registered.
        result = "User Not Found";
    }

    sqlite3_finalize(stmt);
    // Always close the DB connection before returning the login result.
    Database::close(db);
    printf("%s\n", result.c_str());
    return result;
}
