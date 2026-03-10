#include "../include/RedisServer.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

static RedisServer *globalServer = nullptr;

RedisServer::RedisServer(int port) : port(port), server_socket(-1), running(true)
{
    globalServer = this;
}

void RedisServer::shutdown()
{
    running = false;
    if (server_socket != -1)
        close(server_socket);
    std::cout << "Server shutdown complete." << std::endl;
}

void RedisServer::run()
{
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        std::cerr << "Failed to create socket." << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Failed to bind socket." << std::endl;
        return;
    }

    if (listen(server_socket, 10) < 0)
    {
        std::cerr << "Failed to listen on socket." << std::endl;
        return;
    }

    std::cout << "Redis Server is listening on port " << port << "." << std::endl;

    while (running)
    {
        // Accept and handle client connections (omitted for brevity)
    }
}