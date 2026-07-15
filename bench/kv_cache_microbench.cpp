#include "kv_cache.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <cmath>
#include <algorithm>
#include <iomanip>

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

        double sum = 0.0;
        for (int i = 1; i <= n_; ++i) {
            sum += c_ / std::pow(i, theta_);
            if (sum >= d) {
                return i;
            }
        }
        return n_;
    }

private:
    int n_;
    double theta_;
    double c_;
};

struct ThreadResult {
    long long ops = 0;
};

void run_micro_client(KVCache* cache, int duration, int read_ratio, 
                const std::string& distribution, double zipf_theta, int max_keys, ThreadResult& result) {
    
    std::mt19937 gen(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    std::uniform_int_distribution<int> op_dist(1, 100);
    std::uniform_int_distribution<int> key_dist(1, max_keys);
    ZipfianGenerator zipf(max_keys, zipf_theta);

    auto start_time = std::chrono::steady_clock::now();
    long long ops = 0;

    std::string val = "value_1234567890";

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count() >= duration) {
            break;
        }

        int key_idx = (distribution == "zipfian") ? zipf.next(gen) : key_dist(gen);
        std::string key = "key" + std::to_string(key_idx);

        int op = op_dist(gen);
        
        if (op <= read_ratio) {
            std::string outVal;
            cache->get(key, outVal);
        } else {
            cache->set(key, val, 60000); 
        }
        
        ops++;
    }

    result.ops = ops;
}

int main(int argc, char** argv) {
    int clients = 50;
    int duration = 5;
    int read_ratio = 90;
    std::string distribution = "uniform";
    double zipf_theta = 0.99;
    int max_keys = 10000;
    size_t shards = 16;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--clients=") == 0) clients = std::stoi(arg.substr(10));
        else if (arg.find("--duration=") == 0) duration = std::stoi(arg.substr(11));
        else if (arg.find("--read-write-ratio=") == 0) read_ratio = std::stoi(arg.substr(19, arg.find(":") - 19));
        else if (arg.find("--distribution=") == 0) distribution = arg.substr(15);
        else if (arg.find("--zipfian-theta=") == 0) zipf_theta = std::stod(arg.substr(16));
        else if (arg.find("--shards=") == 0) shards = std::stoull(arg.substr(9));
    }

    std::cout << "Starting Microbench: " << clients << " threads, " << duration << " seconds, Shards: " << shards << "\n";
    std::cout << "Distribution: " << distribution << ", Read Ratio: " << read_ratio << "%\n";

    KVCache cache(shards, 100000); 

    std::vector<std::thread> threads;
    std::vector<ThreadResult> results(clients);

    for (int i = 0; i < clients; ++i) {
        threads.emplace_back(run_micro_client, &cache, duration, read_ratio, distribution, zipf_theta, max_keys, std::ref(results[i]));
    }

    for (auto& t : threads) {
        t.join();
    }

    long long total_ops = 0;
    for (const auto& r : results) {
        total_ops += r.ops;
    }

    double throughput = (double)total_ops / duration;

    std::cout << "\nResults:\n";
    std::cout << "Total ops: " << total_ops << "\n";
    std::cout << "Throughput: " << std::fixed << std::setprecision(0) << throughput << " ops/sec\n";

    return 0;
}
