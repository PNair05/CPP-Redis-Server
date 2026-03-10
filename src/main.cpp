#include "../include/RedisServer.h"
#include <iostream>

int main(int argc, char *argv[])
{
    int port = 6379; // Default Redis port
    if (argc >= 2)
        port = std::stoi(argv[1]); // Allow port to be set by the user

    RedisServer server(port);

    return 0;
}