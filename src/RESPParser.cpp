#include "../include/RESPParser.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>

static constexpr size_t READ_BUFFER_SIZE = 4096;

RESPParser::RESPParser(int client_fd) : fd(client_fd) {}

// ---------- Low-level I/O ----------

bool RESPParser::readMore()
{
    char tmp[READ_BUFFER_SIZE];
    ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
    if (n <= 0)
        return false; // Connection closed or error
    buffer.append(tmp, static_cast<size_t>(n));
    return true;
}

bool RESPParser::ensureData(size_t needed)
{
    while (buffer.size() - pos < needed)
    {
        if (!readMore())
            return false;
    }
    return true;
}

std::optional<std::string> RESPParser::readLine()
{
    // Look for \r\n in the buffer starting from pos
    while (true)
    {
        auto crlf = buffer.find("\r\n", pos);
        if (crlf != std::string::npos)
        {
            std::string line = buffer.substr(pos, crlf - pos);
            pos = crlf + 2;
            return line;
        }
        // Need more data
        if (!readMore())
            return std::nullopt;
    }
}

// ---------- Public API ----------

std::optional<RESPValue> RESPParser::parse()
{
    auto result = parseValue();

    // Compact the buffer: discard consumed bytes
    if (pos > 0)
    {
        buffer.erase(0, pos);
        pos = 0;
    }

    return result;
}

// ---------- Recursive descent parser ----------

std::optional<RESPValue> RESPParser::parseValue()
{
    if (!ensureData(1))
        return std::nullopt;

    char type = buffer[pos];

    switch (type)
    {
    case '+':
        return parseSimpleString();
    case '-':
        return parseError();
    case ':':
        return parseInteger();
    case '$':
        return parseBulkString();
    case '*':
        return parseArray();
    default:
        // Inline command support: treat unrecognized data as a
        // space-separated inline command and wrap it as an Array
        // of BulkStrings (e.g. "PING\r\n" -> ["PING"])
        {
            auto line = readLine();
            if (!line)
                return std::nullopt;

            // Prepend the first character we already consumed? No — readLine
            // starts from pos, and we haven't incremented pos past the type byte.
            // Actually, pos still points at 'type'. readLine will include it.
            // We need to back up: the line already contains the type char.

            RESPValue val;
            val.type = RESPType::Array;

            // Tokenize the line by spaces
            std::string token;
            // The line read already consumed from pos, and the type char was
            // part of it since we didn't advance pos. But wait — readLine
            // reads from pos and finds \r\n. The type char IS at pos. So
            // the line will be e.g. "PING" if the input was "PING\r\n".
            // Except we already checked buffer[pos] above but didn't advance.
            // readLine starts from pos, so the type char is included.

            // Actually I need to re-examine. We looked at buffer[pos] but
            // did NOT advance pos. readLine() then reads from pos until \r\n.
            // So the line includes the first char. But that char is not a RESP
            // prefix, so it's part of the inline command. This is correct.

            std::string full = *line;
            size_t start = 0;
            while (start < full.size())
            {
                size_t end = full.find(' ', start);
                if (end == std::string::npos)
                    end = full.size();
                if (end > start)
                {
                    RESPValue elem;
                    elem.type = RESPType::BulkString;
                    elem.str = full.substr(start, end - start);
                    val.array.push_back(std::move(elem));
                }
                start = end + 1;
            }

            return val;
        }
    }
}

std::optional<RESPValue> RESPParser::parseSimpleString()
{
    pos++; // skip '+'
    auto line = readLine();
    if (!line)
        return std::nullopt;

    RESPValue val;
    val.type = RESPType::SimpleString;
    val.str = std::move(*line);
    return val;
}

std::optional<RESPValue> RESPParser::parseError()
{
    pos++; // skip '-'
    auto line = readLine();
    if (!line)
        return std::nullopt;

    RESPValue val;
    val.type = RESPType::Error;
    val.str = std::move(*line);
    return val;
}

std::optional<RESPValue> RESPParser::parseInteger()
{
    pos++; // skip ':'
    auto line = readLine();
    if (!line)
        return std::nullopt;

    RESPValue val;
    val.type = RESPType::Integer;
    val.integer = std::stoll(*line);
    return val;
}

std::optional<RESPValue> RESPParser::parseBulkString()
{
    pos++; // skip '$'
    auto line = readLine();
    if (!line)
        return std::nullopt;

    int64_t len = std::stoll(*line);

    if (len < 0)
    {
        // Null bulk string
        RESPValue val;
        val.type = RESPType::Null;
        val.is_null = true;
        return val;
    }

    // Read exactly `len` bytes + trailing \r\n
    size_t needed = static_cast<size_t>(len) + 2; // data + \r\n
    if (!ensureData(needed))
        return std::nullopt;

    RESPValue val;
    val.type = RESPType::BulkString;
    val.str = buffer.substr(pos, static_cast<size_t>(len));
    pos += static_cast<size_t>(len) + 2; // skip data + \r\n
    return val;
}

std::optional<RESPValue> RESPParser::parseArray()
{
    pos++; // skip '*'
    auto line = readLine();
    if (!line)
        return std::nullopt;

    int64_t count = std::stoll(*line);

    if (count < 0)
    {
        RESPValue val;
        val.type = RESPType::Null;
        val.is_null = true;
        return val;
    }

    RESPValue val;
    val.type = RESPType::Array;
    val.array.reserve(static_cast<size_t>(count));

    for (int64_t i = 0; i < count; i++)
    {
        auto elem = parseValue();
        if (!elem)
            return std::nullopt;
        val.array.push_back(std::move(*elem));
    }

    return val;
}

// ---------- Serialization helpers ----------

std::string RESPParser::serializeSimpleString(const std::string &str)
{
    return "+" + str + "\r\n";
}

std::string RESPParser::serializeError(const std::string &msg)
{
    return "-ERR " + msg + "\r\n";
}

std::string RESPParser::serializeInteger(int64_t val)
{
    return ":" + std::to_string(val) + "\r\n";
}

std::string RESPParser::serializeBulkString(const std::string &str)
{
    return "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n";
}

std::string RESPParser::serializeNullBulkString()
{
    return "$-1\r\n";
}

std::string RESPParser::serializeArray(const std::vector<std::string> &items)
{
    std::string result = "*" + std::to_string(items.size()) + "\r\n";
    for (const auto &item : items)
    {
        result += serializeBulkString(item);
    }
    return result;
}
