#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <cmath>
#include <algorithm>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>

class ZipfianGenerator {
public:
    ZipfianGenerator(int n, double theta) : n_(n), theta_(theta), c_(0.0) {
        for (int i = 1; i <= n_; ++i) {
            c_ += 1.0 / std::pow(i, theta_);
        }
        c_ = 1.0 / c_;
    }

    int next(std::mt19937& gen) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double d = dist(gen);
        double sum = 0;
        for (int i = 1; i <= n_; ++i) {
            sum += c_ / std::pow(i, theta_);
            if (sum >= d) return i;
        }
        return n_;
    }
private:
    int n_;
    double theta_;
    double c_;
};

std::atomic<bool> g_start_flag(false);
std::atomic<bool> g_stop_flag(false);
std::atomic<long long> g_total_ops(0);

struct ThreadResult {
    std::vector<double> latencies;
};

void run_client(const std::string& host, int port, int duration, int read_ratio, 
                const std::string& distribution, double zipf_theta, int max_keys, ThreadResult& result) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(sock);
        return;
    }

    std::mt19937 gen(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    std::uniform_int_distribution<> unif_keys(1, max_keys);
    std::uniform_int_distribution<> op_dist(1, 100);
    std::unique_ptr<ZipfianGenerator> zipf;
    if (distribution == "zipfian") {
        zipf = std::make_unique<ZipfianGenerator>(max_keys, zipf_theta);
    }

    result.latencies.reserve(100000); 

    
    while (!g_start_flag) {
        std::this_thread::yield();
    }

    char buffer[4096];
    while (!g_stop_flag) {
        int key_id;
        if (zipf) {
            key_id = zipf->next(gen);
        } else {
            key_id = unif_keys(gen);
        }
        std::string key = "key" + std::to_string(key_id);

        bool is_read = op_dist(gen) <= read_ratio;
        std::string cmd;
        if (is_read) {
            cmd = "GET " + key + "\r\n";
        } else {
            cmd = "SET " + key + " value" + std::to_string(key_id) + "\r\n";
        }

        auto start = std::chrono::high_resolution_clock::now();
        write(sock, cmd.c_str(), cmd.size());

        ssize_t bytesRead = read(sock, buffer, sizeof(buffer));
        auto end = std::chrono::high_resolution_clock::now();

        if (bytesRead > 0) {
            double ms = std::chrono::duration<double, std::milli>(end - start).count();
            result.latencies.push_back(ms);
            g_total_ops.fetch_add(1, std::memory_order_relaxed);
        }
    }
    close(sock);
}

int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    int port = 6380;
    int clients = 50;
    int duration = 10;
    int read_ratio = 90; 
    std::string distribution = "uniform";
    double zipf_theta = 0.99;
    int max_keys = 10000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--target=") == 0) {
            std::string target = arg.substr(9);
            size_t colon = target.find(':');
            if (colon != std::string::npos) {
                host = target.substr(0, colon);
                if (host == "localhost") host = "127.0.0.1";
                port = std::stoi(target.substr(colon + 1));
            }
        } else if (arg.find("--clients=") == 0) {
            clients = std::stoi(arg.substr(10));
        } else if (arg.find("--duration=") == 0) {
            duration = std::stoi(arg.substr(11));
        } else if (arg.find("--read-write-ratio=") == 0) {
            std::string rw = arg.substr(19);
            size_t colon = rw.find(':');
            if (colon != std::string::npos) {
                read_ratio = std::stoi(rw.substr(0, colon));
            }
        } else if (arg.find("--distribution=") == 0) {
            distribution = arg.substr(15);
        } else if (arg.find("--zipfian-theta=") == 0) {
            zipf_theta = std::stod(arg.substr(16));
        }
    }

    std::cout << "Starting benchmark: " << clients << " clients, " << duration << " seconds\n";
    std::cout << "Distribution: " << distribution << ", Read Ratio: " << read_ratio << "%\n";
    
    
    std::cout << "Initializing generator... (might take a sec for zipfian)\n";
    
    std::vector<ThreadResult> results(clients);
    std::vector<std::thread> threads;
    for (int i = 0; i < clients; ++i) {
        threads.push_back(std::thread(run_client, host, port, duration, read_ratio, distribution, zipf_theta, max_keys, std::ref(results[i])));
    }

    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto start_time = std::chrono::steady_clock::now();
    g_start_flag = true;

    std::this_thread::sleep_for(std::chrono::seconds(duration));
    g_stop_flag = true;

    for (auto& t : threads) {
        t.join();
    }
    
    auto end_time = std::chrono::steady_clock::now();
    double actual_duration = std::chrono::duration<double>(end_time - start_time).count();

    long long ops = g_total_ops.load();
    double tps = ops / actual_duration;

    std::vector<double> all_latencies;
    for (const auto& r : results) {
        all_latencies.insert(all_latencies.end(), r.latencies.begin(), r.latencies.end());
    }

    std::cout << "\nResults:\n";
    std::cout << "Total ops: " << ops << "\n";
    std::cout << "Throughput: " << static_cast<long long>(tps) << " ops/sec\n";

    if (!all_latencies.empty()) {
        std::sort(all_latencies.begin(), all_latencies.end());
        double p50 = all_latencies[all_latencies.size() * 0.50];
        double p95 = all_latencies[all_latencies.size() * 0.95];
        double p99 = all_latencies[all_latencies.size() * 0.99];

        std::cout << "Latency p50: " << p50 << "ms  p95: " << p95 << "ms  p99: " << p99 << "ms\n";
    }

    return 0;
}
