#ifndef KEY_VALUE_STORE_H
#define KEY_VALUE_STORE_H

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <vector>

// Thread-safe in-memory key-value store.
// Uses a readers-writer lock (shared_mutex) so multiple readers
// can access the store concurrently while writes are exclusive.
class KeyValueStore
{
public:
    // Read operations (shared / reader lock)
    std::optional<std::string> get(const std::string &key);
    bool exists(const std::string &key);
    std::vector<std::string> keys();
    size_t dbsize();

    // Write operations (exclusive / writer lock)
    void set(const std::string &key, const std::string &value);
    int del(const std::vector<std::string> &keys);
    void flushdb();

    // Snapshot support — returns a point-in-time copy of the store
    std::unordered_map<std::string, std::string> getSnapshot();

    // Bulk load from a map (used when restoring from a snapshot file)
    void loadFromMap(const std::unordered_map<std::string, std::string> &data);

private:
    std::unordered_map<std::string, std::string> store;
    mutable std::shared_mutex mutex;
};

#endif
