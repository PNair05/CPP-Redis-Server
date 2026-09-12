#include "../include/CommandHandler.h"
#include "../include/KeyValueStore.h"
#include "../include/Snapshot.h"
#include <algorithm>
#include <cctype>

CommandHandler::CommandHandler(KeyValueStore &store, Snapshot &snapshot)
    : store(store), snapshot(snapshot) {}

std::string CommandHandler::handleCommand(const RESPValue &command)
{
    // Commands must be Arrays of BulkStrings
    if (command.type != RESPType::Array || command.array.empty())
    {
        return RESPParser::serializeError("invalid command format");
    }

    // Extract the command name (case-insensitive)
    std::string cmd = command.array[0].str;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                   [](unsigned char c)
                   { return std::toupper(c); });

    if (cmd == "PING")
        return handlePing(command.array);
    if (cmd == "ECHO")
        return handleEcho(command.array);
    if (cmd == "SET")
        return handleSet(command.array);
    if (cmd == "GET")
        return handleGet(command.array);
    if (cmd == "DEL")
        return handleDel(command.array);
    if (cmd == "EXISTS")
        return handleExists(command.array);
    if (cmd == "KEYS")
        return handleKeys(command.array);
    if (cmd == "DBSIZE")
        return handleDbsize(command.array);
    if (cmd == "FLUSHDB")
        return handleFlushdb(command.array);
    if (cmd == "SAVE")
        return handleSave(command.array);
    if (cmd == "BGSAVE")
        return handleBgsave(command.array);

    return RESPParser::serializeError("unknown command '" + command.array[0].str + "'");
}

// ---------- Command implementations ----------

std::string CommandHandler::handlePing(const std::vector<RESPValue> &args)
{
    if (args.size() >= 2)
        return RESPParser::serializeBulkString(args[1].str);
    return RESPParser::serializeSimpleString("PONG");
}

std::string CommandHandler::handleEcho(const std::vector<RESPValue> &args)
{
    if (args.size() < 2)
        return RESPParser::serializeError("wrong number of arguments for 'echo' command");
    return RESPParser::serializeBulkString(args[1].str);
}

std::string CommandHandler::handleSet(const std::vector<RESPValue> &args)
{
    if (args.size() < 3)
        return RESPParser::serializeError("wrong number of arguments for 'set' command");
    store.set(args[1].str, args[2].str);
    return RESPParser::serializeSimpleString("OK");
}

std::string CommandHandler::handleGet(const std::vector<RESPValue> &args)
{
    if (args.size() < 2)
        return RESPParser::serializeError("wrong number of arguments for 'get' command");
    auto val = store.get(args[1].str);
    if (val)
        return RESPParser::serializeBulkString(*val);
    return RESPParser::serializeNullBulkString();
}

std::string CommandHandler::handleDel(const std::vector<RESPValue> &args)
{
    if (args.size() < 2)
        return RESPParser::serializeError("wrong number of arguments for 'del' command");

    std::vector<std::string> keys;
    for (size_t i = 1; i < args.size(); i++)
        keys.push_back(args[i].str);

    int deleted = store.del(keys);
    return RESPParser::serializeInteger(deleted);
}

std::string CommandHandler::handleExists(const std::vector<RESPValue> &args)
{
    if (args.size() < 2)
        return RESPParser::serializeError("wrong number of arguments for 'exists' command");

    int count = 0;
    for (size_t i = 1; i < args.size(); i++)
    {
        if (store.exists(args[i].str))
            count++;
    }
    return RESPParser::serializeInteger(count);
}

std::string CommandHandler::handleKeys(const std::vector<RESPValue> &args)
{
    // Only supports KEYS * (list all keys)
    (void)args;
    auto all_keys = store.keys();
    return RESPParser::serializeArray(all_keys);
}

std::string CommandHandler::handleDbsize(const std::vector<RESPValue> &args)
{
    (void)args;
    return RESPParser::serializeInteger(static_cast<int64_t>(store.dbsize()));
}

std::string CommandHandler::handleFlushdb(const std::vector<RESPValue> &args)
{
    (void)args;
    store.flushdb();
    return RESPParser::serializeSimpleString("OK");
}

std::string CommandHandler::handleSave(const std::vector<RESPValue> &args)
{
    (void)args;
    if (snapshot.save(store))
        return RESPParser::serializeSimpleString("OK");
    return RESPParser::serializeError("snapshot save failed");
}

std::string CommandHandler::handleBgsave(const std::vector<RESPValue> &args)
{
    (void)args;
    if (snapshot.bgsave(store))
        return RESPParser::serializeSimpleString("Background saving started");
    return RESPParser::serializeError("background save already in progress");
}
