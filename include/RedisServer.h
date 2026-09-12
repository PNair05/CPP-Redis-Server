#ifndef REDIS_SERVER_H
#define REDIS_SERVER_H

#include <string>
#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include "KeyValueStore.h"
#include "Snapshot.h"
#include "CommandHandler.h"

class RedisServer
{
public:
    RedisServer(int port);
    ~RedisServer();

    // Start the server — blocks until shutdown
    void run();

    // Initiate graceful shutdown (saves snapshot, closes socket)
    void shutdown();

private:
    int port;
    int server_socket;
    std::atomic<bool> running;

    // Core components
    KeyValueStore store;
    Snapshot snapshot;
    CommandHandler command_handler;

    // Client thread management
    std::vector<std::thread> client_threads;
    std::mutex threads_mutex;

    // Per-client connection handler (runs in its own thread)
    void handleClient(int client_fd);

    // Install SIGINT handler for graceful shutdown
    void setupSignalHandler();
};

#endif
