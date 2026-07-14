#ifndef PROTOCOL_LIMITS_H
#define PROTOCOL_LIMITS_H

#include <cstddef>

/**
 * @file ProtocolLimits.h
 * @brief Shared protocol-facing limits and version constants.
 * @author Muhammad Naheen Mahboob (mmahbo)
 *
 * Declares centralized constants that keep client and server code aligned on
 * payload sizes and protocol compatibility expectations.
 */

/**
 * @namespace ProtocolLimits
 * @brief Shared protocol-facing limits and version constants namespace.
 *
 * Groups the compile-time constants that keep client and server behavior
 * aligned on payload sizing and protocol compatibility.
 */
namespace ProtocolLimits {

/** @brief Maximum raw upload payload size accepted by client-side validation.
 *
 * Defines the largest decoded image payload the client should allow before
 * attempting to send upload data across the network.
 */
static constexpr std::size_t kMaxUploadBytes = 2 * 1024 * 1024;
/** @brief Maximum size intended for embedded view responses on the wire.
 *
 * Defines the upper bound for image data that should be embedded directly in a
 * view response rather than handled through metadata-only signaling.
 */
static constexpr std::size_t kMaxViewEmbedBytes = kMaxUploadBytes;
/** @brief Protocol version tag for compatibility tracking.
 *
 * Identifies the expected marketplace wire-protocol revision so future
 * compatibility checks have a shared constant to reference.
 */
static constexpr int kVersion = 1;

}  // namespace ProtocolLimits

#endif
