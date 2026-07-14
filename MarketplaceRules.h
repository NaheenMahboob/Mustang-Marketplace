#ifndef MARKETPLACE_RULES_H
#define MARKETPLACE_RULES_H

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

/**
 * @file MarketplaceRules.h
 * @brief Shared pure helpers for username/category normalization rules.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */

/**
 * @brief Extracts display username from email prefix.
 * @param email User email.
 * @return Text before `@`, or original input if malformed.
 */
inline std::string usernameFromEmail(const std::string& email) {
    size_t at = email.find('@');
    if (at == std::string::npos || at == 0) return email;
    return email.substr(0, at);
}

/**
 * @brief Normalizes category text to one canonical marketplace category.
 * @param raw User-provided category value.
 * @return Canonical category string or empty string if invalid.
 */
inline std::string normalizeCategory(const std::string& raw) {
    static const std::vector<std::string> kValid = {
        "Textbook", "Furniture", "Electronics", "Clothing", "Sports", "Other"};
    std::string lower = raw;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const std::string& category : kValid) {
        std::string candidate = category;
        std::transform(candidate.begin(), candidate.end(), candidate.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (candidate == lower) return category;
    }
    return "";
}

#endif
