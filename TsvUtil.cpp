/**
 * @file TsvUtil.cpp
 * @brief Shared helpers for serializing safe TSV field values implementation.
 * @author Muhammad Naheen Mahboob (mmahbo)
 */

#include "TsvUtil.h"

std::string tsvEscapeField(const std::string& value) {
    std::string out = value;
    for (char& c : out) {
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    }
    return out;
}
