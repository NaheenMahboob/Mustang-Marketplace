#ifndef SESSION_BRIDGE_H
#define SESSION_BRIDGE_H

#include "User.h"

/**
 * @file SessionBridge.h
 * @brief Adapts token-authenticated API session identity to legacy global session.
 *
 * Declares the bridge helper that keeps older domain services working by
 * copying token-authenticated request identity into `currentSession`.
 * @author Jasmine Jia Gu (jgu284)
 */

/**
 * @brief Maps token-based API session to the global `currentSession` used by legacy service classes.
 *
 * Copies authenticated request identity from the server's token-aware session
 * layer into the legacy process-global session snapshot.
 * @author Jasmine Jia Gu (jgu284)
 *
 * Story 3: Retain Session In Memory
 *
 * @param userId Authenticated user id from token/session map.
 * @param email Authenticated user email from token/session map.
 */
inline void syncSessionForNetwork(int userId, const std::string& email) {
    currentSession.active = true;
    currentSession.user_id = userId;
    currentSession.email = email;
}

#endif
