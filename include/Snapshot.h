#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstdint>

class KeyValueStore;

// Handles persistent snapshotting of the key-value store to disk.
// Supports synchronous (SAVE) and background (BGSAVE) modes.
//
// File format (custom binary):
//   [magic: "RDBCPP\0\0" (8 bytes)]
//   [entry_count: uint64]
//   For each entry:
//     [key_len: uint32] [key_data] [val_len: uint32] [val_data]
//   [checksum: uint64 (FNV-1a hash of all preceding bytes)]
class Snapshot
{
public:
    explicit Snapshot(const std::string &filename = "dump.rdb");

    // Synchronous save — blocks until complete
    bool save(KeyValueStore &store);

    // Background save — takes a snapshot copy, then writes in a detached thread
    bool bgsave(KeyValueStore &store);

    // Load snapshot from disk into the store
    bool load(KeyValueStore &store);

    bool isSaving() const { return saving.load(); }

private:
    std::string filename;
    std::atomic<bool> saving{false};
    std::mutex save_mutex; // Prevent concurrent saves

    bool writeToFile(const std::unordered_map<std::string, std::string> &data);
    bool readFromFile(std::unordered_map<std::string, std::string> &data);

    // FNV-1a hash for checksum
    static uint64_t fnv1aHash(const void *data, size_t len, uint64_t hash = 14695981039346656037ULL);

    static constexpr char MAGIC[8] = {'R', 'D', 'B', 'C', 'P', 'P', '\0', '\0'};
    static constexpr size_t MAGIC_SIZE = 8;
};

#endif
