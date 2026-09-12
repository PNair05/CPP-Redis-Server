#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include <vector>
#include "RESPParser.h"

class KeyValueStore;
class Snapshot;

// Dispatches parsed RESP commands to the appropriate store and
// snapshot operations, returning RESP-encoded responses.
class CommandHandler
{
public:
    CommandHandler(KeyValueStore &store, Snapshot &snapshot);

    // Takes a parsed RESP command (expected to be an Array of BulkStrings)
    // and returns the RESP-encoded response string.
    std::string handleCommand(const RESPValue &command);

private:
    KeyValueStore &store;
    Snapshot &snapshot;

    std::string handlePing(const std::vector<RESPValue> &args);
    std::string handleEcho(const std::vector<RESPValue> &args);
    std::string handleSet(const std::vector<RESPValue> &args);
    std::string handleGet(const std::vector<RESPValue> &args);
    std::string handleDel(const std::vector<RESPValue> &args);
    std::string handleExists(const std::vector<RESPValue> &args);
    std::string handleKeys(const std::vector<RESPValue> &args);
    std::string handleDbsize(const std::vector<RESPValue> &args);
    std::string handleFlushdb(const std::vector<RESPValue> &args);
    std::string handleSave(const std::vector<RESPValue> &args);
    std::string handleBgsave(const std::vector<RESPValue> &args);
};

#endif
