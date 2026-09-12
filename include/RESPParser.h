#ifndef RESP_PARSER_H
#define RESP_PARSER_H

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

// Represents the type of a parsed RESP value
enum class RESPType
{
    SimpleString,
    Error,
    Integer,
    BulkString,
    Array,
    Null
};

// A single parsed RESP value (recursive for arrays)
struct RESPValue
{
    RESPType type;
    std::string str;              // For SimpleString, Error, BulkString
    int64_t integer = 0;          // For Integer
    std::vector<RESPValue> array; // For Array
    bool is_null = false;         // For Null bulk strings / arrays
};

// Buffered RESP parser that reads from a socket file descriptor
class RESPParser
{
public:
    explicit RESPParser(int client_fd);

    // Parse the next RESP value from the socket.
    // Returns std::nullopt on connection close or parse error.
    std::optional<RESPValue> parse();

    // --- Serialization helpers (RESP encoding) ---
    static std::string serializeSimpleString(const std::string &str);
    static std::string serializeError(const std::string &msg);
    static std::string serializeInteger(int64_t val);
    static std::string serializeBulkString(const std::string &str);
    static std::string serializeNullBulkString();
    static std::string serializeArray(const std::vector<std::string> &items);

private:
    int fd;
    std::string buffer;
    size_t pos = 0;

    // Low-level I/O
    bool readMore();
    bool ensureData(size_t needed);
    std::optional<std::string> readLine();

    // Recursive descent parsers for each RESP type
    std::optional<RESPValue> parseValue();
    std::optional<RESPValue> parseSimpleString();
    std::optional<RESPValue> parseError();
    std::optional<RESPValue> parseInteger();
    std::optional<RESPValue> parseBulkString();
    std::optional<RESPValue> parseArray();
};

#endif
