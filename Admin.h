// Admin.h
// MustangMarketplace — Group 49
// Story 15

#ifndef ADMIN_H
#define ADMIN_H

#include <string>
#include "Database.h"

/**
 * @file Admin.h
 * @brief Admin-only moderation operations.
 *
 * Declares administrative moderation helpers used to validate elevated access
 * and apply ban actions to user accounts.
 * @author Samuel Ross Wobschall (swobscha)
 */

/**
 * @class Admin
 * @brief Administrative actions.
 *
 * Groups the moderation operations that are reserved for users who present the
 * configured admin access code.
 * @author Samuel Ross Wobschall (swobscha)
 */
class Admin {
private:
    /** @brief Shared access code expected from admin clients.
     *
     * Stores the fixed administrative secret checked before privileged actions
     * are allowed to proceed.
     */
    static const std::string ACCESS_CODE;

public:
    // Story 15: Ban User
    // PASS: valid user     -> "User banned"
    // FAIL: user not found -> "User Not Found"
    /**
     * @brief Bans a user account.
     *
     * Marks the targeted user as banned so subsequent business operations can
     * deny access according to moderation rules.
     * @author Samuel Ross Wobschall (swobscha)
     * @param user_id Target user id.
     * @return Operation status string.
     */
    static std::string banUser(int user_id);

    /**
     * @brief Validates admin access code.
     *
     * Compares a submitted access code against the configured admin secret
     * before a privileged operation is executed.
     * @author Samuel Ross Wobschall (swobscha)
     * @param code Candidate code from request.
     * @return True when code matches configured admin code.
     */
    static bool validateCode(const std::string& code);
};

#endif
