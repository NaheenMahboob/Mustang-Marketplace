/**
 * @file Photo.cpp
 * @brief Listing photo model and server-side image storage/view operations implementation.
 *
 * Implements the photo domain object plus the helpers that attach, store, and
 * retrieve listing photos for both legacy and raw-byte API flows.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @author Samuel Ross Wobschall (swobscha)
 * @author Ashwin Subash (asubash2)
 */

#include <string>
using namespace std;

#include "Photo.h"
#include "User.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

static string g_assetRoot;  // Project-root base path used to resolve stored relative photo paths.

/**
 * @brief Resolves stored DB path to an absolute filesystem path.
 *
 * Converts a stored relative photo path into a normalized filesystem path
 * rooted at the configured asset directory when necessary.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param stored Relative or absolute stored photo path.
 * @return Absolute/normalized path when possible.
 */
static string resolveStoredPath(const string& stored) {
    // Normalize relative DB paths under configured asset root.
    if (stored.empty()) return stored;
    if (!stored.empty() && stored[0] == '/') return stored;
    if (g_assetRoot.empty()) return stored;
    return (fs::path(g_assetRoot) / stored).lexically_normal().string();
}

void Photo::setAssetRoot(const string& absoluteProjectDir) {
    g_assetRoot = absoluteProjectDir;
    // Trim trailing separators so later relative-path joins remain predictable.
    while (!g_assetRoot.empty() && (g_assetRoot.back() == '/' || g_assetRoot.back() == '\\')) g_assetRoot.pop_back();
}

// Simple value-object constructor used when photo rows are materialized from
// the database into an in-memory snapshot.
Photo::Photo(int pid, int lid, const string& path) : photo_id(pid), listing_id(lid), file_path(path) {}
// Return the persisted photo id stored on this snapshot.
int Photo::getPhotoId() const { return photo_id; }
// Return the listing id that owns this photo snapshot.
int Photo::getListingId() const { return listing_id; }
// Return the stored relative or absolute path for this photo snapshot.
string Photo::getFilePath() const { return file_path; }

// Resolve one listing's stored photo into the lightweight legacy
// "Photo: <path>" / "Placeholder" status contract.
string Photo::view(int listing_id) {
    sqlite3* db = Database::open();  // Database handle used for loading the stored photo path.
    if (!db) return "DB Error";

    const string sql = "SELECT file_path FROM photos WHERE listing_id = ? LIMIT 1;";  // Query that fetches the stored photo path for a listing.
    sqlite3_stmt* stmt = nullptr;  // Prepared statement for the photo-path lookup.
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(stmt, 1, listing_id);

    string result;  // Final view-photo result string returned to the caller.
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        string stored = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));  // File path exactly as stored in the database.
        string resolved = resolveStoredPath(stored);  // Absolute or normalized path used for the filesystem probe.
        // Probe the resolved file so broken paths can fall back to the placeholder flow.
        FILE* f = fopen(resolved.c_str(), "rb");
        if (f) {
            fclose(f);
            result = "Photo: " + stored;
        } else {
            // Missing files still return the placeholder rather than a hard DB error.
            result = "Placeholder";
        }
    } else {
        // Listings with no photo row also use the placeholder behavior.
        result = "Placeholder";
    }
    sqlite3_finalize(stmt);
    Database::close(db);
    return result;
}

/**
 * @brief Infers output extension from image magic bytes.
 *
 * Examines the leading bytes of decoded image content and chooses a likely
 * filename extension for persisted upload files.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param raw Raw decoded image bytes.
 * @return File extension including leading dot.
 */
static string pickExtension(const vector<uint8_t>& raw) {
    if (raw.size() >= 3 && raw[0] == 0xff && raw[1] == 0xd8 && raw[2] == 0xff) return ".jpg";
    if (raw.size() >= 8 && raw[0] == 0x89 && raw[1] == 'P' && raw[2] == 'N' && raw[3] == 'G') return ".png";
    if (raw.size() >= 12 && raw[0] == 'R' && raw[1] == 'I' && raw[2] == 'F' && raw[3] == 'F') return ".webp";
    return ".bin";
}

/**
 * @brief Verifies listing exists, is active, and is owned by current user.
 *
 * Centralizes the validation shared by photo mutation paths so only editable
 * listings owned by the authenticated seller can be changed.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param db Open SQLite connection.
 * @param listing_id Listing id being edited.
 * @return Empty string on success, otherwise domain error string.
 */
static string verifyEditableListing(sqlite3* db, int listing_id) {
    // Shared authorization gate for photo mutation endpoints.
    sqlite3_stmt* checkStmt = nullptr;  // Prepared statement for checking listing ownership and status.
    if (sqlite3_prepare_v2(db, "SELECT seller_id, status FROM listings WHERE listing_id = ?;", -1, &checkStmt, nullptr) != SQLITE_OK) {
        return "DB Error";
    }
    sqlite3_bind_int(checkStmt, 1, listing_id);
    if (sqlite3_step(checkStmt) != SQLITE_ROW) {
        sqlite3_finalize(checkStmt);
        return "Item Not Found";
    }
    int seller_id = sqlite3_column_int(checkStmt, 0);  // Seller id owning the listing being edited.
    string status = reinterpret_cast<const char*>(sqlite3_column_text(checkStmt, 1));  // Current listing status used to enforce editability.
    sqlite3_finalize(checkStmt);
    // Photo writes are limited to the listing owner.
    if (seller_id != currentSession.user_id) return "Access Denied";
    // Only active listings may be modified.
    if (status != "active") return "Item Not Available";
    return "";
}

/**
 * @brief Unlinks a stored path only when it belongs to managed uploads.
 *
 * Prevents accidental deletion of arbitrary files by only removing paths that
 * were created under the server-managed uploads directory.
 * @author Muhammad Naheen Mahboob (mmahbo)
 * @param stored Stored path from DB.
 * @return None.
 */
static void unlinkIfUploadPath(const string& stored) {
    if (stored.empty()) return;
    if (stored.rfind("uploads/", 0) != 0) return;
    // Only server-managed upload files are physically removed from disk.
    unlink(resolveStoredPath(stored).c_str());
}

string Photo::add(int listing_id, const string& file_path) {
    if (!currentSession.active) return "Session Expired";
    sqlite3* db = Database::open();  // Database handle used for path-based photo replacement/removal.
    if (!db) return "DB Error";
    string chk = verifyEditableListing(db, listing_id);  // Editability result for the target listing.
    if (!chk.empty()) {
        Database::close(db);
        return chk;
    }

    // If we're removing or replacing an upload, capture existing stored path first
    string oldStored;  // Previously stored photo path so replaced managed uploads can be cleaned up.
    sqlite3_stmt* sel = nullptr;  // Prepared statement for loading any existing photo path.
    if (sqlite3_prepare_v2(db, "SELECT file_path FROM photos WHERE listing_id = ? LIMIT 1;", -1, &sel, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(sel, 1, listing_id);
        if (sqlite3_step(sel) == SQLITE_ROW) oldStored = reinterpret_cast<const char*>(sqlite3_column_text(sel, 0));
        sqlite3_finalize(sel);
    }

    // Keep photos one-per-listing by clearing any existing row first.
    sqlite3_stmt* del = nullptr;  // Prepared statement that deletes any existing photo row for this listing.
    if (sqlite3_prepare_v2(db, "DELETE FROM photos WHERE listing_id = ?;", -1, &del, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(del, 1, listing_id);
    // Clear any prior row so the replacement path becomes the sole stored photo entry.
    sqlite3_step(del);
    sqlite3_finalize(del);
    if (file_path.empty()) {
        Database::close(db);
        // Empty path is the explicit "remove photo" command.
        unlinkIfUploadPath(oldStored);
        return "Photo removed";
    }

    sqlite3_stmt* ins = nullptr;  // Prepared statement that inserts the replacement path-based photo row.
    if (sqlite3_prepare_v2(db, "INSERT INTO photos (listing_id, file_path) VALUES (?, ?);", -1, &ins, nullptr) != SQLITE_OK) {
        Database::close(db);
        return "DB Error";
    }
    sqlite3_bind_int(ins, 1, listing_id);
    sqlite3_bind_text(ins, 2, file_path.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(ins) == SQLITE_DONE);  // True when the replacement path row was stored successfully.
    sqlite3_finalize(ins);
    Database::close(db);
    // Remove any superseded managed upload after the DB row has been replaced.
    unlinkIfUploadPath(oldStored);
    return ok ? "Photo path set" : "DB Error";
}

string Photo::saveRaw(int listing_id, const string& raw_bytes) {
    if (!currentSession.active) return "Session Expired";
    if (raw_bytes.empty()) return "Invalid image data";
    if (raw_bytes.size() > MAX_UPLOAD_BYTES) return "Image too large";

    sqlite3* db = Database::open();  // Database handle used for ownership checks and photo-row replacement.
    if (!db) return "DB Error";
    string chk = verifyEditableListing(db, listing_id);  // Editability result for the target listing.
    if (!chk.empty()) {
        Database::close(db);
        return chk;
    }

    // Capture previous photo path to clean up later.
    string oldStored;  // Previously stored photo path so replaced managed uploads can be cleaned up.
    sqlite3_stmt* sel = nullptr;  // Prepared statement for loading any existing photo path.
    if (sqlite3_prepare_v2(db, "SELECT file_path FROM photos WHERE listing_id = ? LIMIT 1;", -1, &sel, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(sel, 1, listing_id);
        if (sqlite3_step(sel) == SQLITE_ROW) oldStored = reinterpret_cast<const char*>(sqlite3_column_text(sel, 0));
        sqlite3_finalize(sel);
    }

    // Generate unique filename with extension inferred from the uploaded bytes.
    ostringstream name;  // Filename builder for the new managed upload path.
    name << "uploads/l" << listing_id << "_" << time(nullptr) << "_" << (rand() & 0xffffff);
    name << pickExtension(vector<uint8_t>(raw_bytes.begin(), raw_bytes.end()));
    const string rel = name.str();  // Relative uploads/ path saved into the database.
    const string full = resolveStoredPath(rel);  // Absolute/normalized path used for the actual file write.
    error_code ec;  // Non-throwing filesystem error sink for directory creation.
    fs::create_directories(fs::path(full).parent_path(), ec);
    ofstream out(full, ios::binary | ios::trunc);  // Output stream that writes the raw upload bytes to disk.
    if (!out || !out.write(raw_bytes.data(), raw_bytes.size())) {
        Database::close(db);
        return "DB Error";
    }

    // Replace the current row atomically as delete-then-insert for one listing id.
    sqlite3_stmt* del = nullptr;  // Prepared statement that clears any existing photo row for this listing.
    if (sqlite3_prepare_v2(db, "DELETE FROM photos WHERE listing_id = ?;", -1, &del, nullptr) != SQLITE_OK) {
        Database::close(db);
        unlink(full.c_str());
        return "DB Error";
    }
    sqlite3_bind_int(del, 1, listing_id);
    // Remove any prior row so the replacement upload stays one-photo-per-listing.
    sqlite3_step(del);
    sqlite3_finalize(del);

    // Insert the replacement managed photo row after the new file is safely written.
    sqlite3_stmt* ins = nullptr;  // Prepared statement that inserts the new managed photo row.
    if (sqlite3_prepare_v2(db, "INSERT INTO photos (listing_id, file_path) VALUES (?, ?);", -1, &ins, nullptr) != SQLITE_OK) {
        Database::close(db);
        unlink(full.c_str());
        return "DB Error";
    }
    sqlite3_bind_int(ins, 1, listing_id);
    sqlite3_bind_text(ins, 2, rel.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(ins) == SQLITE_DONE);  // True when the new managed photo row was stored successfully.
    sqlite3_finalize(ins);
    Database::close(db);
    if (!ok) {
        // Roll back the just-written file if the DB insert failed.
        unlink(full.c_str());
        return "DB Error";
    }
    // Clean up the superseded managed upload after the new row is committed.
    if (!oldStored.empty() && oldStored.rfind("uploads/", 0) == 0) unlink(resolveStoredPath(oldStored).c_str());
    return "Photo saved";
}

bool Photo::loadRaw(int listing_id, std::string& outBytes, std::string& outContentType) {
    outBytes.clear();
    outContentType.clear();

    sqlite3* db = Database::open();  // Database handle used for the initial photo lookup.
    if (!db) return false;

    sqlite3_stmt* stmt = nullptr;  // Prepared statement for fetching the listing's stored photo path.
    const char* sql = "SELECT file_path FROM photos WHERE listing_id = ? LIMIT 1;";  // Query that fetches the stored photo path for a listing.
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Database::close(db);
        return false;
    }
    sqlite3_bind_int(stmt, 1, listing_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        Database::close(db);
        return false;
    }

    string stored = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));  // File path exactly as stored in the database.
    sqlite3_finalize(stmt);
    Database::close(db);

    // Resolve the stored path to an absolute filesystem path before reading bytes.
    string resolved = resolveStoredPath(stored);  // Absolute or normalized path used for reading the image file.
    ifstream in(resolved, ios::binary | ios::ate);  // Input stream opened on the resolved image file.
    if (!in) return false;

    streamsize sz = in.tellg();  // Total image size used for buffer allocation.
    if (sz <= 0) return false;
    in.seekg(0, ios::beg);
    outBytes.resize(static_cast<size_t>(sz));  // Output buffer sized to the full image payload.
    if (!in.read(&outBytes[0], sz)) return false;

    // Infer the HTTP response content type from the resolved filename extension.
    string ext;  // Lower-level file extension used to choose the HTTP content type.
    size_t dot = resolved.rfind('.');
    if (dot != string::npos) ext = resolved.substr(dot);
    if (ext == ".jpg" || ext == ".jpeg") outContentType = "image/jpeg";
    else if (ext == ".png") outContentType = "image/png";
    else if (ext == ".webp") outContentType = "image/webp";
    else outContentType = "application/octet-stream";

    return true;
}
