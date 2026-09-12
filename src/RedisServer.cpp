#include "../include/RedisServer.h"
#include "../include/RESPParser.h"
#include <csignal>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// Global pointer for signal handler access
static RedisServer *globalServer = nullptr;

static void signalHandler(int signum) {
  (void)signum;
  std::cout << "\nReceived shutdown signal..." << std::endl;
  if (globalServer)
    globalServer->shutdown();
}

RedisServer::RedisServer(int port)
    : port(port), server_socket(-1), running(false),
      command_handler(store, snapshot) {
  globalServer = this;
}

RedisServer::~RedisServer() {
  if (running.load())
    shutdown();

  // Join any remaining client threads
  std::lock_guard<std::mutex> lock(threads_mutex);
  for (auto &t : client_threads) {
    if (t.joinable())
      t.join();
  }
}

void RedisServer::setupSignalHandler() {
  struct sigaction sa;
  std::memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
}

void RedisServer::run() {
  setupSignalHandler();

  // Load existing snapshot if present
  snapshot.load(store);

  // Create TCP socket
  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) {
    std::cerr << "Failed to create socket." << std::endl;
    return;
  }

  // Allow port reuse
  int opt = 1;
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // Bind to address
  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(server_socket, reinterpret_cast<struct sockaddr *>(&server_addr),
           sizeof(server_addr)) < 0) {
    std::cerr << "Failed to bind socket on port " << port << "." << std::endl;
    close(server_socket);
    return;
  }

  // Start listening (backlog of 128 pending connections)
  if (listen(server_socket, 128) < 0) {
    std::cerr << "Failed to listen on socket." << std::endl;
    close(server_socket);
    return;
  }

  running = true;
  std::cout << "Redis Server is listening on port " << port << "." << std::endl;

  // Main accept loop
  while (running.load()) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    int client_fd =
        accept(server_socket, reinterpret_cast<struct sockaddr *>(&client_addr),
               &client_len);
    if (client_fd < 0) {
      if (running.load())
        std::cerr << "Failed to accept connection." << std::endl;
      continue; // On shutdown, accept returns -1 — just loop and exit
    }

    // Spawn a thread to handle this client
    std::lock_guard<std::mutex> lock(threads_mutex);

    // Clean up finished threads before adding new ones
    client_threads.erase(std::remove_if(client_threads.begin(),
                                        client_threads.end(),
                                        [](std::thread &t) {
                                          if (t.joinable()) {
                                            // Try to join threads that might be
                                            // done We detach here since we
                                            // can't non-blocking join in C++
                                            // The thread cleans up on its own
                                            return false;
                                          }
                                          return true;
                                        }),
                         client_threads.end());

    client_threads.emplace_back(&RedisServer::handleClient, this, client_fd);
  }
}

void RedisServer::handleClient(int client_fd) {
  RESPParser parser(client_fd);

  while (running.load()) {
    auto command = parser.parse();
    if (!command)
      break; // Client disconnected or parse error

    std::string response = command_handler.handleCommand(*command);

    // Send the response
    size_t total_sent = 0;
    while (total_sent < response.size()) {
      ssize_t sent = send(client_fd, response.data() + total_sent,
                          response.size() - total_sent, 0);
      if (sent <= 0)
        break;
      total_sent += static_cast<size_t>(sent);
    }

    if (total_sent < response.size())
      break; // Send failed, client likely disconnected
  }

  close(client_fd);
}

void RedisServer::shutdown() {
  if (!running.exchange(false))
    return; // Already shutting down

  std::cout << "Saving snapshot before shutdown..." << std::endl;
  snapshot.save(store);

  if (server_socket != -1) {
    close(server_socket);
    server_socket = -1;
  }

  std::cout << "Server shutdown complete." << std::endl;
}
