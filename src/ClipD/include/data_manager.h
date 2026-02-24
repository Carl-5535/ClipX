#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "common/windows.h"
#include <sqlite3.h>
#include "common/types.h"

namespace clipx {

class DataManager {
public:
    static DataManager& Instance();

    bool Initialize(const std::string& dbPath);
    void Shutdown();

    // Insert a new clipboard entry to database
    int64_t Insert(const ClipboardEntry& entry);

    // Insert entry to memory only (not persisted until tagged)
    int64_t InsertMemoryOnly(const ClipboardEntry& entry);

    // Get entry from memory or database
    std::optional<ClipboardEntry> GetEntry(int64_t id);

    // Persist memory entry to database
    // Returns the new database ID if successful
    std::optional<int64_t> PersistMemoryEntry(int64_t memoryId);

    // Clear all memory entries
    void ClearMemoryEntries();

    // Query history with options
    std::vector<ClipboardEntry> Query(const QueryOptions& options);

    // Search by keyword (includes both memory and database)
    std::vector<ClipboardEntry> Search(const std::string& keyword, int limit = 50);

    // Get full data for an entry
    std::vector<uint8_t> GetEntryData(int64_t id);

    // Delete entry
    bool Delete(int64_t id);

    // Delete entries older than timestamp
    int DeleteOlderThan(int64_t timestamp);

    // Delete all entries
    bool DeleteAll();

    // Toggle favorite
    bool ToggleFavorite(int64_t id);

    // Check if hash exists (for deduplication)
    std::optional<int64_t> FindByHash(const std::vector<uint8_t>& hash);

    // Update existing entry (increment copy count)
    bool UpdateCopyCount(int64_t id, int64_t newTimestamp);

    // Get statistics
    DatabaseStats GetStats();

    // Set clipboard content from entry
    bool SetClipboard(int64_t id);

    // Tag operations
    bool AddTag(int64_t entryId, const std::string& tagName);
    bool RemoveTag(int64_t entryId, const std::string& tagName);
    std::vector<std::string> GetTags(int64_t entryId);
    std::vector<std::pair<std::string, int>> GetAllTags();  // Returns tag name and count

    // Cleanup orphaned tag records (tags pointing to deleted entries)
    // Returns the number of orphaned records cleaned up
    int CleanupOrphanedTags();

    // Check if initialized
    bool IsInitialized() const { return m_initialized; }

private:
    DataManager() = default;
    ~DataManager();

    DataManager(const DataManager&) = delete;
    DataManager& operator=(const DataManager&) = delete;

    bool CreateTables();
    bool UpgradeSchema();
    ClipboardEntry RowToEntry(sqlite3_stmt* stmt);
    void LoadTagsForEntry(ClipboardEntry& entry);

    sqlite3* m_db = nullptr;
    std::mutex m_mutex;
    bool m_initialized = false;
    std::string m_dbPath;

    // Memory storage for non-tagged entries
    std::vector<ClipboardEntry> m_memoryEntries;
    int64_t m_nextMemoryId = -1;  // Negative IDs for memory entries
};

} // namespace clipx
