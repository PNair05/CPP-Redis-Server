#include "../include/KeyValueStore.h"

std::optional<std::string> KeyValueStore::get(const std::string &key)
{
    std::shared_lock lock(mutex);
    auto it = store.find(key);
    if (it != store.end())
        return it->second;
    return std::nullopt;
}

bool KeyValueStore::exists(const std::string &key)
{
    std::shared_lock lock(mutex);
    return store.count(key) > 0;
}

std::vector<std::string> KeyValueStore::keys()
{
    std::shared_lock lock(mutex);
    std::vector<std::string> result;
    result.reserve(store.size());
    for (const auto &[key, _] : store)
    {
        result.push_back(key);
    }
    return result;
}

size_t KeyValueStore::dbsize()
{
    std::shared_lock lock(mutex);
    return store.size();
}

void KeyValueStore::set(const std::string &key, const std::string &value)
{
    std::unique_lock lock(mutex);
    store[key] = value;
}

int KeyValueStore::del(const std::vector<std::string> &keys)
{
    std::unique_lock lock(mutex);
    int count = 0;
    for (const auto &key : keys)
    {
        count += static_cast<int>(store.erase(key));
    }
    return count;
}

void KeyValueStore::flushdb()
{
    std::unique_lock lock(mutex);
    store.clear();
}

std::unordered_map<std::string, std::string> KeyValueStore::getSnapshot()
{
    std::shared_lock lock(mutex);
    return store; // Returns a copy while holding the shared lock
}

void KeyValueStore::loadFromMap(const std::unordered_map<std::string, std::string> &data)
{
    std::unique_lock lock(mutex);
    store = data;
}
