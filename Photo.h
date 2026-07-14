// Photo.h
// MustangMarketplace — Group 49
// Stories 5, 14, 16

#ifndef PHOTO_H
#define PHOTO_H

#include <string>
#include <vector>

#include "Database.h"

/**
 * @file Photo.h
 * @brief Listing photo model and server-side image storage/view operations.
 *
 * Declares the photo domain object plus the helpers that attach, store, and
 * retrieve listing photos for both legacy and raw-byte API flows.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Samuel Ross Wobschall (swobscha)
 * @author Ashwin Subash (asubash2)
 */

/**
 * @class Photo
 * @brief Photo entity and related service methods.
 *
 * Represents one stored photo snapshot and exposes the operations that manage
 * file-backed listing images on the server.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Samuel Ross Wobschall (swobscha)
 * @author Ashwin Subash (asubash2)
 */
class Photo {
private:
    /** Persisted photo id for this snapshot. */
    int photo_id;
    /** Listing id that owns this photo snapshot. */
    int listing_id;
    /** Stored filesystem or relative asset path for this photo snapshot. */
    std::string file_path;

public:
    /** @brief Server-enforced raw upload limit in bytes.
     *
     * Defines the maximum raw image size accepted during upload handling.
     */
    static constexpr std::size_t MAX_UPLOAD_BYTES = 2 * 1024 * 1024;

    /**
     * @brief Constructs a photo object snapshot.
     *
     * Copies the supplied photo row fields into an in-memory object for simple
     * inspection and transport.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @param pid Photo id.
     * @param lid Listing id.
     * @param path Stored photo path.
     */
    Photo(int pid, int lid, const std::string& path);

    /** @brief Returns photo id.
     *
     * Provides the persisted identifier for this photo snapshot.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @return Photo id value.
     */
    int getPhotoId() const;
    /** @brief Returns listing id that owns this photo.
     *
     * Provides the listing id associated with this photo snapshot.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @return Owning listing id value.
     */
    int getListingId() const;
    /** @brief Returns stored photo file path.
     *
     * Provides the stored filesystem path or relative asset path for this
     * photo snapshot.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @return Stored photo path string.
     */
    std::string getFilePath() const;

    /**
     * @brief Loads raw image bytes for a listing.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @param listing_id Listing id whose photo should be loaded.
     * @param outBytes Output raw image bytes.
     * @param outContentType Output MIME type (e.g., image/jpeg).
     * @return True if the image exists and was loaded successfully.
     */
    static bool loadRaw(int listing_id, std::string& outBytes, std::string& outContentType);

    /**
     * @brief Saves raw image bytes directly to disk.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @param listing_id Listing id to attach image to.
     * @param raw_bytes Raw image bytes.
     * @return Operation status string.
     */
    static std::string saveRaw(int listing_id, const std::string& raw_bytes);

    // Story 16: View Listing Photo
    // PASS: file exists    -> "Photo: <path>"
    // FAIL: broken/missing -> "Placeholder"
    /**
     * @brief Returns human-readable photo view message for legacy flows.
     *
     * Resolves the stored photo path for a listing and returns a lightweight
     * status string suitable for legacy text-based flows.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @param listing_id Listing id.
     * @return `Photo: <path>` or placeholder-style status.
     */
    static std::string view(int listing_id);
    // Supports Stories 5 and 14: Listing photo add/edit/remove flows
    /**
     * @brief Adds/replaces/removes listing photo by path semantics.
     *
     * Applies a path-based photo change for the target listing, including
     * replacement and explicit removal behavior when the path is empty.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @author Samuel Ross Wobschall (swobscha)
     * @author Ashwin Subash (asubash2)
     * @param listing_id Listing id to update.
     * @param file_path New file path; empty means remove.
     * @return Operation status string.
     */
    static std::string add(int listing_id, const std::string& file_path);
    /**
     * @brief Sets absolute asset root used to resolve relative upload paths.
     *
     * Stores the server-side project root used when converting relative photo
     * paths into filesystem locations.
     * @author Muhammad Naheen Mahboob (mmahbo)
     * @author Samuel Ross Wobschall (swobscha)
     * @author Ashwin Subash (asubash2)
     * @param absoluteProjectDir Absolute project directory.
     */
    static void setAssetRoot(const std::string& absoluteProjectDir);
};

#endif
