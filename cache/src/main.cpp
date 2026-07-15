#include "kv_cache.h"
#include "tcp_server.h"
#include "ttl_sweeper.h"
#include "logger.h"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>

std::atomic<bool> g_stop_requested(false);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_stop_requested = true;
    }
}

int main(int argc, char** argv) {
    int port = 6380;
    size_t shards = 16;
    size_t max_entries_per_shard = 1000;
    int sweep_interval_ms = 1000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--port=") == 0) {
            port = std::stoi(arg.substr(7));
        } else if (arg.find("--shards=") == 0) {
            shards = std::stoull(arg.substr(9));
        } else if (arg.find("--max-entries-per-shard=") == 0) {
            max_entries_per_shard = std::stoull(arg.substr(24));
        } else if (arg.find("--sweep-interval-ms=") == 0) {
            sweep_interval_ms = std::stoi(arg.substr(20));
        }
    }

    Logger::log("Starting KV Cache Server...");
    Logger::log("Port: " + std::to_string(port));
    Logger::log("Shards: " + std::to_string(shards));
    Logger::log("Max entries per shard: " + std::to_string(max_entries_per_shard));

    KVCache cache(shards, max_entries_per_shard);
    TcpServer server(port, cache);
    TtlSweeper sweeper(cache, sweep_interval_ms);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    server.start();
    sweeper.start();

    while (!g_stop_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    Logger::log("Shutting down...");
    server.stop();
    sweeper.stop();
    Logger::log("Shutdown complete.");

    return 0;
}
