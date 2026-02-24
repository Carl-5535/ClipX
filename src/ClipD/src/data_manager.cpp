#include "common/windows.h"
#include "data_manager.h"
#include "common/logger.h"
#include "common/utils.h"
#include <sstream>
#include <algorithm>
#include <cstring>

// Define DROPFILES locally if not available
#ifndef DROPFILES
typedef struct _DROPFILES {
    DWORD pFiles;
    POINT pt;
    BOOL fNC;
    BOOL fWide;
} DROPFILES, *LPDROPFILES;
#endif

namespace clipx {

DataManager& DataManager::Instance() {
    static DataManager instance;
    return instance;
}

DataManager::~DataManager() {
    Shutdown();
}

bool DataManager::Initialize(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return true;
    }

    m_dbPath = dbPath;

    // Open database
    int result = sqlite3_open(dbPath.c_str(), &m_db);
    if (result != SQLITE_OK) {
        LOG_ERROR("Failed to open database: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    // Enable WAL mode for better performance
    sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

    // Create tables
    if (!CreateTables()) {
        LOG_ERROR("Failed to create database tables");
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    m_initialized = true;
    LOG_INFO("DataManager initialized: " + dbPath);
    return true;
}

void DataManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
    m_initialized = false;
    LOG_INFO("DataManager shutdown");
}

bool DataManager::CreateTables() {
    const char* createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS clipboard_entries (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp       INTEGER NOT NULL,
            type            INTEGER NOT NULL,
            data            BLOB NOT NULL,
            preview         TEXT,
            source_app      TEXT,
            hash            BLOB NOT NULL,
            copy_count      INTEGER DEFAULT 1,
            is_favorited    INTEGER DEFAULT 0,
            is_tagged       INTEGER DEFAULT 0,
            created_at      INTEGER NOT NULL,
            updated_at      INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS entry_tags (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            entry_id        INTEGER NOT NULL,
            tag_name        TEXT NOT NULL,
            created_at      INTEGER NOT NULL,
            FOREIGN KEY (entry_id) REFERENCES clipboard_entries(id) ON DELETE CASCADE,
            UNIQUE(entry_id, tag_name)
        );

        CREATE INDEX IF NOT EXISTS idx_timestamp ON clipboard_entries(timestamp DESC);
        CREATE INDEX IF NOT EXISTS idx_type ON clipboard_entries(type);
        CREATE INDEX IF NOT EXISTS idx_hash ON clipboard_entries(hash);
        CREATE INDEX IF NOT EXISTS idx_favorited ON clipboard_entries(is_favorited);
        CREATE INDEX IF NOT EXISTS idx_entry_tags_entry ON entry_tags(entry_id);
    )";

    char* errorMsg = nullptr;
    int result = sqlite3_exec(m_db, createTableSQL, nullptr, nullptr, &errorMsg);
    if (result != SQLITE_OK) {
        LOG_ERROR("SQL error: " + std::string(errorMsg ? errorMsg : "unknown"));
        if (errorMsg) sqlite3_free(errorMsg);
        return false;
    }

    // Upgrade schema if needed (adds is_tagged column for existing tables)
    if (!UpgradeSchema()) {
        return false;
    }

    // Create index on is_tagged after schema upgrade
    const char* createTaggedIndexSQL = "CREATE INDEX IF NOT EXISTS idx_tagged ON clipboard_entries(is_tagged)";
    result = sqlite3_exec(m_db, createTaggedIndexSQL, nullptr, nullptr, &errorMsg);
    if (result != SQLITE_OK) {
        LOG_ERROR("Failed to create idx_tagged: " + std::string(errorMsg ? errorMsg : "unknown"));
        if (errorMsg) sqlite3_free(errorMsg);
        // Non-fatal, continue
    }

    return true;
}

bool DataManager::UpgradeSchema() {
    // Check if is_tagged column exists
    const char* checkColSQL = "PRAGMA table_info(clipboard_entries)";
    sqlite3_stmt* stmt = nullptr;
    bool hasIsTagged = false;

    if (sqlite3_prepare_v2(m_db, checkColSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* colName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            if (colName && strcmp(colName, "is_tagged") == 0) {
                hasIsTagged = true;
                break;
            }
        }
        sqlite3_finalize(stmt);
    }

    // Add is_tagged column if it doesn't exist
    if (!hasIsTagged) {
        const char* alterSQL = "ALTER TABLE clipboard_entries ADD COLUMN is_tagged INTEGER DEFAULT 0";
        char* errorMsg = nullptr;
        int result = sqlite3_exec(m_db, alterSQL, nullptr, nullptr, &errorMsg);
        if (result != SQLITE_OK) {
            LOG_ERROR("Failed to add is_tagged column: " + std::string(errorMsg ? errorMsg : "unknown"));
            if (errorMsg) sqlite3_free(errorMsg);
            // Non-fatal, continue
        } else {
            LOG_INFO("Added is_tagged column to clipboard_entries");
        }
    }

    return true;
}

int64_t DataManager::Insert(const ClipboardEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return -1;

    const char* sql = R"(
        INSERT INTO clipboard_entries (timestamp, type, data, preview, source_app, hash, copy_count, is_favorited, is_tagged, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare insert statement: " + std::string(sqlite3_errmsg(m_db)));
        return -1;
    }

    int64_t now = utils::GetCurrentTimestamp();
    std::vector<uint8_t> hash = utils::ComputeHash(entry.data);

    sqlite3_bind_int64(stmt, 1, entry.timestamp);
    sqlite3_bind_int(stmt, 2, static_cast<int>(entry.type));
    sqlite3_bind_blob(stmt, 3, entry.data.data(), static_cast<int>(entry.data.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, entry.preview.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, entry.sourceApp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 6, hash.data(), static_cast<int>(hash.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, entry.copyCount);
    sqlite3_bind_int(stmt, 8, entry.isFavorited ? 1 : 0);
    sqlite3_bind_int(stmt, 9, entry.isTagged ? 1 : 0);
    sqlite3_bind_int64(stmt, 10, now);
    sqlite3_bind_int64(stmt, 11, now);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        LOG_ERROR("Failed to insert entry: " + std::string(sqlite3_errmsg(m_db)));
        return -1;
    }

    int64_t id = sqlite3_last_insert_rowid(m_db);
    LOG_DEBUG("Inserted entry with id: " + std::to_string(id));
    return id;
}

int64_t DataManager::InsertMemoryOnly(const ClipboardEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);

    ClipboardEntry memoryEntry = entry;
    memoryEntry.id = m_nextMemoryId--;
    memoryEntry.isTagged = false;

    m_memoryEntries.insert(m_memoryEntries.begin(), memoryEntry);

    // Limit memory entries to prevent excessive memory usage
    const size_t maxMemoryEntries = 100;
    if (m_memoryEntries.size() > maxMemoryEntries) {
        m_memoryEntries.resize(maxMemoryEntries);
    }

    LOG_DEBUG("Inserted memory entry with id: " + std::to_string(memoryEntry.id));
    return memoryEntry.id;
}

void DataManager::ClearMemoryEntries() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_memoryEntries.clear();
    m_nextMemoryId = -1;
    LOG_INFO("Cleared all memory entries");
}

std::optional<int64_t> DataManager::PersistMemoryEntry(int64_t memoryId) {
    // Find the memory entry
    auto it = std::find_if(m_memoryEntries.begin(), m_memoryEntries.end(),
        [memoryId](const ClipboardEntry& e) { return e.id == memoryId; });

    if (it == m_memoryEntries.end()) {
        return std::nullopt;  // Not a memory entry
    }

    // Insert to database
    ClipboardEntry entry = *it;
    entry.isTagged = true;

    const char* sql = R"(
        INSERT INTO clipboard_entries (timestamp, type, data, preview, source_app, hash, copy_count, is_favorited, is_tagged, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare persist statement: " + std::string(sqlite3_errmsg(m_db)));
        return std::nullopt;
    }

    int64_t now = utils::GetCurrentTimestamp();
    std::vector<uint8_t> hash = utils::ComputeHash(entry.data);

    sqlite3_bind_int64(stmt, 1, entry.timestamp);
    sqlite3_bind_int(stmt, 2, static_cast<int>(entry.type));
    sqlite3_bind_blob(stmt, 3, entry.data.data(), static_cast<int>(entry.data.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, entry.preview.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, entry.sourceApp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 6, hash.data(), static_cast<int>(hash.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, entry.copyCount);
    sqlite3_bind_int(stmt, 8, entry.isFavorited ? 1 : 0);
    sqlite3_bind_int(stmt, 9, 1);  // is_tagged = 1
    sqlite3_bind_int64(stmt, 10, now);
    sqlite3_bind_int64(stmt, 11, now);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        LOG_ERROR("Failed to persist entry: " + std::string(sqlite3_errmsg(m_db)));
        return std::nullopt;
    }

    int64_t newId = sqlite3_last_insert_rowid(m_db);

    // Remove from memory (entry is now persisted)
    m_memoryEntries.erase(it);

    LOG_INFO("Persisted memory entry to database with id: " + std::to_string(newId));
    return newId;
}

ClipboardEntry DataManager::RowToEntry(sqlite3_stmt* stmt) {
    ClipboardEntry entry;
    entry.id = sqlite3_column_int64(stmt, 0);
    entry.timestamp = sqlite3_column_int64(stmt, 1);
    entry.type = static_cast<ClipboardDataType>(sqlite3_column_int(stmt, 2));

    const void* dataBlob = sqlite3_column_blob(stmt, 3);
    int dataSize = sqlite3_column_bytes(stmt, 3);
    entry.data.assign(static_cast<const uint8_t*>(dataBlob),
                      static_cast<const uint8_t*>(dataBlob) + dataSize);

    const char* preview = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    entry.preview = preview ? preview : "";

    const char* sourceApp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    entry.sourceApp = sourceApp ? sourceApp : "";

    entry.copyCount = sqlite3_column_int(stmt, 6);
    entry.isFavorited = sqlite3_column_int(stmt, 7) != 0;

    // Check if is_tagged column exists (column 8)
    if (sqlite3_column_count(stmt) > 8) {
        entry.isTagged = sqlite3_column_int(stmt, 8) != 0;
    }

    return entry;
}

void DataManager::LoadTagsForEntry(ClipboardEntry& entry) {
    if (!m_db || entry.id < 0) return;

    const char* sql = "SELECT tag_name FROM entry_tags WHERE entry_id = ? ORDER BY created_at";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }

    sqlite3_bind_int64(stmt, 1, entry.id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* tagName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (tagName) {
            entry.tags.push_back(tagName);
        }
    }

    sqlite3_finalize(stmt);
}

std::vector<ClipboardEntry> DataManager::Query(const QueryOptions& options) {
    std::vector<ClipboardEntry> entries;
    std::lock_guard<std::mutex> lock(m_mutex);

    // Add memory entries
    for (const auto& memEntry : m_memoryEntries) {
        if (options.filterType.has_value() && memEntry.type != *options.filterType) {
            continue;
        }
        if (options.favoritesOnly && !memEntry.isFavorited) {
            continue;
        }
        entries.push_back(memEntry);
    }

    if (!m_initialized) return entries;

    // Query database entries
    std::ostringstream sql;
    sql << "SELECT id, timestamp, type, data, preview, source_app, copy_count, is_favorited, is_tagged FROM clipboard_entries";

    bool hasWhere = false;
    if (options.filterType.has_value()) {
        sql << " WHERE type = " << static_cast<int>(*options.filterType);
        hasWhere = true;
    }
    if (options.favoritesOnly) {
        sql << (hasWhere ? " AND" : " WHERE") << " is_favorited = 1";
        hasWhere = true;
    }

    // Order by
    switch (options.sortOrder) {
        case QueryOptions::SortOrder::LatestFirst:
            sql << " ORDER BY timestamp DESC";
            break;
        case QueryOptions::SortOrder::OldestFirst:
            sql << " ORDER BY timestamp ASC";
            break;
        case QueryOptions::SortOrder::MostCopied:
            sql << " ORDER BY copy_count DESC, timestamp DESC";
            break;
        case QueryOptions::SortOrder::Alphabetical:
            sql << " ORDER BY preview ASC";
            break;
    }

    sql << " LIMIT " << options.limit << " OFFSET " << options.offset;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare query: " + std::string(sqlite3_errmsg(m_db)));
        return entries;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ClipboardEntry entry = RowToEntry(stmt);
        LoadTagsForEntry(entry);
        entries.push_back(entry);
    }

    sqlite3_finalize(stmt);

    // Sort all entries (both memory and database) together
    switch (options.sortOrder) {
        case QueryOptions::SortOrder::LatestFirst:
            std::sort(entries.begin(), entries.end(),
                [](const ClipboardEntry& a, const ClipboardEntry& b) {
                    return a.timestamp > b.timestamp;
                });
            break;
        case QueryOptions::SortOrder::OldestFirst:
            std::sort(entries.begin(), entries.end(),
                [](const ClipboardEntry& a, const ClipboardEntry& b) {
                    return a.timestamp < b.timestamp;
                });
            break;
        case QueryOptions::SortOrder::MostCopied:
            std::sort(entries.begin(), entries.end(),
                [](const ClipboardEntry& a, const ClipboardEntry& b) {
                    if (a.copyCount != b.copyCount) {
                        return a.copyCount > b.copyCount;
                    }
                    return a.timestamp > b.timestamp;
                });
            break;
        case QueryOptions::SortOrder::Alphabetical:
            std::sort(entries.begin(), entries.end(),
                [](const ClipboardEntry& a, const ClipboardEntry& b) {
                    return a.preview < b.preview;
                });
            break;
    }

    // Apply offset and limit after sorting
    size_t startIdx = std::min(static_cast<size_t>(options.offset), entries.size());
    size_t endIdx = std::min(startIdx + static_cast<size_t>(options.limit), entries.size());

    if (startIdx > 0 || endIdx < entries.size()) {
        entries = std::vector<ClipboardEntry>(entries.begin() + startIdx, entries.begin() + endIdx);
    }

    return entries;
}

std::vector<ClipboardEntry> DataManager::Search(const std::string& keyword, int limit) {
    std::vector<ClipboardEntry> entries;
    std::lock_guard<std::mutex> lock(m_mutex);

    if (keyword.empty()) return entries;

    // Search in memory entries (both preview and tags)
    std::string searchLower = keyword;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    for (const auto& memEntry : m_memoryEntries) {
        bool found = false;

        // Search in preview text
        std::string previewLower = memEntry.preview;
        std::transform(previewLower.begin(), previewLower.end(), previewLower.begin(), ::tolower);

        if (previewLower.find(searchLower) != std::string::npos) {
            found = true;
        }

        // Search in tags
        if (!found) {
            for (const auto& tag : memEntry.tags) {
                std::string tagLower = tag;
                std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(), ::tolower);
                if (tagLower.find(searchLower) != std::string::npos) {
                    found = true;
                    break;
                }
            }
        }

        if (found) {
            entries.push_back(memEntry);
            if (static_cast<int>(entries.size()) >= limit) {
                return entries;
            }
        }
    }

    if (!m_initialized) return entries;

    // Search in database - search both preview text and tags
    // Use DISTINCT to avoid duplicates when an entry matches both preview and tag
    std::string sql = R"(
        SELECT DISTINCT e.id, e.timestamp, e.type, e.data, e.preview, e.source_app, e.copy_count, e.is_favorited, e.is_tagged
        FROM clipboard_entries e
        LEFT JOIN entry_tags t ON e.id = t.entry_id
        WHERE e.preview LIKE ? OR t.tag_name LIKE ?
        ORDER BY e.timestamp DESC
        LIMIT ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare search: " + std::string(sqlite3_errmsg(m_db)));
        return entries;
    }

    // Use %keyword% for substring matching
    std::string searchPattern = "%" + keyword + "%";

    sqlite3_bind_text(stmt, 1, searchPattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, searchPattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, limit - static_cast<int>(entries.size()));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ClipboardEntry entry = RowToEntry(stmt);
        LoadTagsForEntry(entry);
        entries.push_back(entry);
    }

    sqlite3_finalize(stmt);
    return entries;
}

std::optional<ClipboardEntry> DataManager::GetEntry(int64_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check memory first for negative IDs
    if (id < 0) {
        auto it = std::find_if(m_memoryEntries.begin(), m_memoryEntries.end(),
            [id](const ClipboardEntry& e) { return e.id == id; });
        if (it != m_memoryEntries.end()) {
            return *it;
        }
        return std::nullopt;
    }

    if (!m_initialized) return std::nullopt;

    const char* sql = "SELECT id, timestamp, type, data, preview, source_app, copy_count, is_favorited, is_tagged FROM clipboard_entries WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare get entry: " + std::string(sqlite3_errmsg(m_db)));
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, id);

    std::optional<ClipboardEntry> entry;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        entry = RowToEntry(stmt);
        if (entry) {
            LoadTagsForEntry(*entry);
        }
    }

    sqlite3_finalize(stmt);
    return entry;
}

std::vector<uint8_t> DataManager::GetEntryData(int64_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return {};

    const char* sql = "SELECT data FROM clipboard_entries WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare get data: " + std::string(sqlite3_errmsg(m_db)));
        return {};
    }

    sqlite3_bind_int64(stmt, 1, id);

    std::vector<uint8_t> data;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* blob = sqlite3_column_blob(stmt, 0);
        int size = sqlite3_column_bytes(stmt, 0);
        data.assign(static_cast<const uint8_t*>(blob),
                    static_cast<const uint8_t*>(blob) + size);
    }

    sqlite3_finalize(stmt);
    return data;
}

bool DataManager::Delete(int64_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if it's a memory entry (negative ID)
    if (id < 0) {
        auto it = std::find_if(m_memoryEntries.begin(), m_memoryEntries.end(),
            [id](const ClipboardEntry& e) { return e.id == id; });

        if (it != m_memoryEntries.end()) {
            m_memoryEntries.erase(it);
            LOG_DEBUG("Deleted memory entry: " + std::to_string(id));
            return true;
        }
        return false;
    }

    // Database entry
    if (!m_initialized) return false;

    const char* sql = "DELETE FROM clipboard_entries WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare delete: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, id);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        LOG_ERROR("Failed to delete entry: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    LOG_DEBUG("Deleted database entry: " + std::to_string(id));
    return true;
}

int DataManager::DeleteOlderThan(int64_t timestamp) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return 0;

    const char* sql = "DELETE FROM clipboard_entries WHERE timestamp < ? AND is_favorited = 0";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare delete older than: " + std::string(sqlite3_errmsg(m_db)));
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, timestamp);

    int result = sqlite3_step(stmt);
    int deleted = static_cast<int>(sqlite3_changes(m_db));
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        LOG_ERROR("Failed to delete old entries: " + std::string(sqlite3_errmsg(m_db)));
        return 0;
    }

    LOG_INFO("Deleted " + std::to_string(deleted) + " old entries");
    return deleted;
}

bool DataManager::DeleteAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return false;

    int result = sqlite3_exec(m_db, "DELETE FROM clipboard_entries", nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("Failed to delete all entries: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    // Vacuum to reclaim space
    sqlite3_exec(m_db, "VACUUM", nullptr, nullptr, nullptr);

    LOG_INFO("Deleted all entries");
    return true;
}

bool DataManager::ToggleFavorite(int64_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return false;

    const char* sql = "UPDATE clipboard_entries SET is_favorited = NOT is_favorited, updated_at = ? WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare toggle favorite: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, utils::GetCurrentTimestamp());
    sqlite3_bind_int64(stmt, 2, id);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        LOG_ERROR("Failed to toggle favorite: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    LOG_DEBUG("Toggled favorite for entry: " + std::to_string(id));
    return true;
}

std::optional<int64_t> DataManager::FindByHash(const std::vector<uint8_t>& hash) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return std::nullopt;

    const char* sql = "SELECT id FROM clipboard_entries WHERE hash = ? ORDER BY timestamp DESC LIMIT 1";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare find by hash: " + std::string(sqlite3_errmsg(m_db)));
        return std::nullopt;
    }

    sqlite3_bind_blob(stmt, 1, hash.data(), static_cast<int>(hash.size()), SQLITE_TRANSIENT);

    std::optional<int64_t> id;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int64(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return id;
}

bool DataManager::UpdateCopyCount(int64_t id, int64_t newTimestamp) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return false;

    const char* sql = "UPDATE clipboard_entries SET copy_count = copy_count + 1, timestamp = ?, updated_at = ? WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare update copy count: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    int64_t now = utils::GetCurrentTimestamp();
    sqlite3_bind_int64(stmt, 1, newTimestamp);
    sqlite3_bind_int64(stmt, 2, now);
    sqlite3_bind_int64(stmt, 3, id);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        LOG_ERROR("Failed to update copy count: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    return true;
}

DatabaseStats DataManager::GetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);

    DatabaseStats stats;
    if (!m_initialized) return stats;

    // Get total count
    const char* countSql = "SELECT COUNT(*) FROM clipboard_entries";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, countSql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.totalCount = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // Get size by type
    const char* sizeSql = "SELECT type, SUM(LENGTH(data)) FROM clipboard_entries GROUP BY type";
    if (sqlite3_prepare_v2(m_db, sizeSql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int type = sqlite3_column_int(stmt, 0);
            int64_t size = sqlite3_column_int64(stmt, 1);
            if (type == static_cast<int>(ClipboardDataType::Image)) {
                stats.imageSize = size;
            } else {
                stats.textSize += size;
            }
            stats.totalSize += size;
        }
        sqlite3_finalize(stmt);
    }

    return stats;
}

bool DataManager::SetClipboard(int64_t id) {
    auto entry = GetEntry(id);
    if (!entry) {
        LOG_ERROR("Entry not found: " + std::to_string(id));
        return false;
    }

    if (!OpenClipboard(nullptr)) {
        LOG_ERROR("Failed to open clipboard");
        return false;
    }

    EmptyClipboard();

    bool success = false;

    switch (entry->type) {
        case ClipboardDataType::Text:
        case ClipboardDataType::Html:
        case ClipboardDataType::Rtf: {
            std::wstring wtext = utils::Utf8ToWide(
                std::string(entry->data.begin(), entry->data.end())
            );
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (wtext.size() + 1) * sizeof(wchar_t));
            if (hMem) {
                wchar_t* ptr = static_cast<wchar_t*>(GlobalLock(hMem));
                if (ptr) {
                    wcscpy_s(ptr, wtext.size() + 1, wtext.c_str());
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                    success = true;
                }
            }
            break;
        }
        case ClipboardDataType::Files: {
            // Parse file list (null-separated paths)
            std::wstring paths;
            size_t offset = 0;
            while (offset < entry->data.size()) {
                size_t end = offset;
                while (end < entry->data.size() && entry->data[end] != 0) {
                    end++;
                }
                if (end > offset) {
                    std::string path(entry->data.begin() + offset, entry->data.begin() + end);
                    if (!paths.empty()) paths += L'\0';
                    paths += utils::Utf8ToWide(path);
                }
                offset = end + 1;
            }

            if (!paths.empty()) {
                paths = L'\0' + paths + L'\0';
                size_t size = sizeof(DROPFILES) + (paths.size() + 1) * sizeof(wchar_t);
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
                if (hMem) {
                    DROPFILES* df = static_cast<DROPFILES*>(GlobalLock(hMem));
                    if (df) {
                        df->pFiles = sizeof(DROPFILES);
                        df->fWide = TRUE;
                        wcscpy_s(reinterpret_cast<wchar_t*>(df + 1), paths.size() + 1, paths.c_str());
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_HDROP, hMem);
                        success = true;
                    }
                }
            }
            break;
        }
        default:
            // For other types, just set as text
            std::wstring wtext = utils::Utf8ToWide(entry->preview);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (wtext.size() + 1) * sizeof(wchar_t));
            if (hMem) {
                wchar_t* ptr = static_cast<wchar_t*>(GlobalLock(hMem));
                if (ptr) {
                    wcscpy_s(ptr, wtext.size() + 1, wtext.c_str());
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                    success = true;
                }
            }
            break;
    }

    CloseClipboard();
    LOG_DEBUG("Set clipboard from entry: " + std::to_string(id));
    return success;
}

bool DataManager::AddTag(int64_t entryId, const std::string& tagName) {
    // If this is a memory entry, persist it first
    if (entryId < 0) {
        auto newId = PersistMemoryEntry(entryId);
        if (!newId.has_value()) {
            LOG_ERROR("Failed to persist memory entry for tagging: " + std::to_string(entryId));
            return false;
        }
        entryId = *newId;  // Use the new database ID
        LOG_INFO("Memory entry persisted with new ID: " + std::to_string(entryId));
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return false;

    // First, set is_tagged = 1 for the entry
    const char* updateSql = "UPDATE clipboard_entries SET is_tagged = 1, updated_at = ? WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, updateSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, utils::GetCurrentTimestamp());
        sqlite3_bind_int64(stmt, 2, entryId);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Insert tag (IGNORE to handle duplicates)
    const char* insertSql = "INSERT OR IGNORE INTO entry_tags (entry_id, tag_name, created_at) VALUES (?, ?, ?)";
    if (sqlite3_prepare_v2(m_db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare add tag: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, entryId);
    sqlite3_bind_text(stmt, 2, tagName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, utils::GetCurrentTimestamp());

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        LOG_ERROR("Failed to add tag: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    LOG_INFO("Added tag '" + tagName + "' to entry: " + std::to_string(entryId));
    return true;
}

bool DataManager::RemoveTag(int64_t entryId, const std::string& tagName) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized || entryId < 0) return false;

    const char* sql = "DELETE FROM entry_tags WHERE entry_id = ? AND tag_name = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare remove tag: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, entryId);
    sqlite3_bind_text(stmt, 2, tagName.c_str(), -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        LOG_ERROR("Failed to remove tag: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    // Check if entry has any remaining tags
    const char* countSql = "SELECT COUNT(*) FROM entry_tags WHERE entry_id = ?";
    if (sqlite3_prepare_v2(m_db, countSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, entryId);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0);
            if (count == 0) {
                // No more tags, set is_tagged = 0
                sqlite3_finalize(stmt);
                const char* updateSql = "UPDATE clipboard_entries SET is_tagged = 0, updated_at = ? WHERE id = ?";
                if (sqlite3_prepare_v2(m_db, updateSql, -1, &stmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_int64(stmt, 1, utils::GetCurrentTimestamp());
                    sqlite3_bind_int64(stmt, 2, entryId);
                    sqlite3_step(stmt);
                    sqlite3_finalize(stmt);
                }
            } else {
                sqlite3_finalize(stmt);
            }
        } else {
            sqlite3_finalize(stmt);
        }
    }

    LOG_INFO("Removed tag '" + tagName + "' from entry: " + std::to_string(entryId));
    return true;
}

std::vector<std::string> DataManager::GetTags(int64_t entryId) {
    std::vector<std::string> tags;
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized || entryId < 0) return tags;

    const char* sql = "SELECT tag_name FROM entry_tags WHERE entry_id = ? ORDER BY created_at";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare get tags: " + std::string(sqlite3_errmsg(m_db)));
        return tags;
    }

    sqlite3_bind_int64(stmt, 1, entryId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* tagName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (tagName) {
            tags.push_back(tagName);
        }
    }

    sqlite3_finalize(stmt);
    return tags;
}

std::vector<std::pair<std::string, int>> DataManager::GetAllTags() {
    std::vector<std::pair<std::string, int>> tags;
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return tags;

    // Use INNER JOIN to only count tags that have valid entries
    // This filters out orphaned tag records that reference deleted entries
    const char* sql = R"(
        SELECT t.tag_name, COUNT(*) as count
        FROM entry_tags t
        INNER JOIN clipboard_entries e ON t.entry_id = e.id
        GROUP BY t.tag_name
        ORDER BY count DESC, t.tag_name ASC
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare get all tags: " + std::string(sqlite3_errmsg(m_db)));
        return tags;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* tagName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int count = sqlite3_column_int(stmt, 1);
        if (tagName) {
            tags.push_back({tagName, count});
        }
    }

    sqlite3_finalize(stmt);
    LOG_DEBUG("GetAllTags returned " + std::to_string(tags.size()) + " tags");
    return tags;
}

int DataManager::CleanupOrphanedTags() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return 0;

    // Delete tag records that reference non-existent entries
    const char* sql = R"(
        DELETE FROM entry_tags
        WHERE entry_id NOT IN (SELECT id FROM clipboard_entries)
    )";

    char* errorMsg = nullptr;
    int result = sqlite3_exec(m_db, sql, nullptr, nullptr, &errorMsg);

    int deletedCount = 0;
    if (result == SQLITE_OK) {
        deletedCount = static_cast<int>(sqlite3_changes(m_db));
        LOG_INFO("Cleaned up " + std::to_string(deletedCount) + " orphaned tag records");
    } else {
        LOG_ERROR("Failed to cleanup orphaned tags: " + std::string(errorMsg ? errorMsg : "unknown"));
        if (errorMsg) sqlite3_free(errorMsg);
    }

    return deletedCount;
}

} // namespace clipx
