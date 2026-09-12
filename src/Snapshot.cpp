#include "../include/Snapshot.h"
#include "../include/KeyValueStore.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <thread>

Snapshot::Snapshot(const std::string &filename) : filename(filename) {}

uint64_t Snapshot::fnv1aHash(const void *data, size_t len, uint64_t hash)
{
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < len; i++)
    {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool Snapshot::save(KeyValueStore &store)
{
    std::lock_guard<std::mutex> lock(save_mutex);
    saving = true;

    // Take a point-in-time snapshot (shared lock on the store)
    auto data = store.getSnapshot();
    bool result = writeToFile(data);

    saving = false;
    return result;
}

bool Snapshot::bgsave(KeyValueStore &store)
{
    if (saving.load())
    {
        std::cerr << "Background save already in progress." << std::endl;
        return false;
    }

    // Take a snapshot copy while briefly holding the shared lock
    auto data = store.getSnapshot();

    // Write the snapshot in a detached background thread
    std::thread([this, data = std::move(data)]() mutable
                {
        std::lock_guard<std::mutex> lock(save_mutex);
        saving = true;

        std::cout << "Background saving started." << std::endl;
        bool ok = writeToFile(data);
        if (ok)
            std::cout << "Background saving completed successfully." << std::endl;
        else
            std::cerr << "Background saving failed." << std::endl;

        saving = false; })
        .detach();

    return true;
}

bool Snapshot::load(KeyValueStore &store)
{
    std::unordered_map<std::string, std::string> data;
    if (!readFromFile(data))
        return false;

    store.loadFromMap(data);
    std::cout << "Loaded " << data.size() << " keys from " << filename << "." << std::endl;
    return true;
}

bool Snapshot::writeToFile(const std::unordered_map<std::string, std::string> &data)
{
    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        std::cerr << "Failed to open " << filename << " for writing." << std::endl;
        return false;
    }

    uint64_t checksum = 14695981039346656037ULL; // FNV offset basis

    // Write magic bytes
    out.write(MAGIC, MAGIC_SIZE);
    checksum = fnv1aHash(MAGIC, MAGIC_SIZE, checksum);

    // Write entry count
    uint64_t count = data.size();
    out.write(reinterpret_cast<const char *>(&count), sizeof(count));
    checksum = fnv1aHash(&count, sizeof(count), checksum);

    // Write each key-value pair
    for (const auto &[key, value] : data)
    {
        uint32_t key_len = static_cast<uint32_t>(key.size());
        out.write(reinterpret_cast<const char *>(&key_len), sizeof(key_len));
        checksum = fnv1aHash(&key_len, sizeof(key_len), checksum);

        out.write(key.data(), key_len);
        checksum = fnv1aHash(key.data(), key_len, checksum);

        uint32_t val_len = static_cast<uint32_t>(value.size());
        out.write(reinterpret_cast<const char *>(&val_len), sizeof(val_len));
        checksum = fnv1aHash(&val_len, sizeof(val_len), checksum);

        out.write(value.data(), val_len);
        checksum = fnv1aHash(value.data(), val_len, checksum);
    }

    // Write checksum
    out.write(reinterpret_cast<const char *>(&checksum), sizeof(checksum));

    out.flush();
    if (!out.good())
    {
        std::cerr << "Error writing snapshot to " << filename << "." << std::endl;
        return false;
    }

    return true;
}

bool Snapshot::readFromFile(std::unordered_map<std::string, std::string> &data)
{
    std::ifstream in(filename, std::ios::binary);
    if (!in)
        return false; // File doesn't exist — not an error on startup

    uint64_t checksum = 14695981039346656037ULL;

    // Verify magic bytes
    char magic[MAGIC_SIZE];
    in.read(magic, MAGIC_SIZE);
    if (!in || std::memcmp(magic, MAGIC, MAGIC_SIZE) != 0)
    {
        std::cerr << "Invalid snapshot file: bad magic bytes." << std::endl;
        return false;
    }
    checksum = fnv1aHash(magic, MAGIC_SIZE, checksum);

    // Read entry count
    uint64_t count;
    in.read(reinterpret_cast<char *>(&count), sizeof(count));
    if (!in)
        return false;
    checksum = fnv1aHash(&count, sizeof(count), checksum);

    // Read entries
    for (uint64_t i = 0; i < count; i++)
    {
        uint32_t key_len;
        in.read(reinterpret_cast<char *>(&key_len), sizeof(key_len));
        if (!in)
            return false;
        checksum = fnv1aHash(&key_len, sizeof(key_len), checksum);

        std::string key(key_len, '\0');
        in.read(key.data(), key_len);
        if (!in)
            return false;
        checksum = fnv1aHash(key.data(), key_len, checksum);

        uint32_t val_len;
        in.read(reinterpret_cast<char *>(&val_len), sizeof(val_len));
        if (!in)
            return false;
        checksum = fnv1aHash(&val_len, sizeof(val_len), checksum);

        std::string value(val_len, '\0');
        in.read(value.data(), val_len);
        if (!in)
            return false;
        checksum = fnv1aHash(value.data(), val_len, checksum);

        data[std::move(key)] = std::move(value);
    }

    // Verify checksum
    uint64_t stored_checksum;
    in.read(reinterpret_cast<char *>(&stored_checksum), sizeof(stored_checksum));
    if (!in || stored_checksum != checksum)
    {
        std::cerr << "Snapshot checksum mismatch — file may be corrupt." << std::endl;
        data.clear();
        return false;
    }

    return true;
}
