#ifndef TSV_UTIL_H
#define TSV_UTIL_H

#include <string>

/**
 * @file TsvUtil.h
 * @brief Shared helpers for serializing safe TSV field values.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */

/**
 * @brief Replaces TSV-breaking control characters with spaces.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param value Input field.
 * @return Escaped field safe for tab/newline-separated payloads.
 */
std::string tsvEscapeField(const std::string& value);

#endif
