# Project: Concurrent Sharded In-Memory KV Cache (with Product Lookup API Demo)

## 1. What this project is

A high-performance, concurrent, in-memory key-value cache server written in C++
(a simplified Redis/Memcached), designed to sit in front of a real database and
absorb read traffic. It supports many simultaneous client connections, uses sharded
locking to minimize contention, evicts old entries with LRU when full, and expires
entries automatically via TTL.

To make the project demoable and give it a genuine real-world story (not just
synthetic benchmarks), it's paired with a **thin product-lookup API** backed by
**SQLite**, so you can show the classic cache-aside pattern live: first request to a
product is slow (hits disk), second request is dramatically faster (served from
cache).

### Core features (build in this order — each should be solid before moving to the next)
1. Thread-safe single-shard hashmap (`get`/`set`/`delete`)
2. Sharding across N independent shards, each with its own lock
3. LRU eviction per shard (O(1) via hashmap + intrusive doubly linked list)
4. TTL expiry (lazy on read + active background sweep)
5. TCP server exposing a simple text protocol so external processes can talk to it
6. A CLI client (`kv_cli`) for manual testing/demoing
7. A benchmarking tool (`kv_bench`) that hits the server with many concurrent
   clients and reports throughput/latency
8. The product-lookup API + SQLite integration (cache-aside pattern)
9. Cold-vs-warm latency comparison script/output

### Stretch goals (only after 1-9 are solid)
- `STATS` command (hit rate, miss rate, evictions, current memory usage)
- **Rate limiter built on the cache itself** (see section 4a) — track requests per
  client IP using the cache's own `SET`/`GET`/TTL primitives, and reject requests
  that exceed a threshold (e.g. >100 req/sec). Stronger than a plain auth token
  because it demonstrates a second real use of TTL and adds a genuine
  network-security angle to the resume bullet.
- Config file instead of only CLI flags
- Simple write-through persistence (periodic snapshot to disk)

### Explicitly out of scope
- Cluster mode / multiple cache nodes / replication (that's the Raft project's job)
- A frontend UI of any kind
- A production-grade wire protocol (RESP) — a simple custom text protocol is fine
  and is easier to demo/debug

---

## 2. Architecture overview

```
                     ┌──────────────────┐
   kv_cli    ───────►│                  │
   (manual demo)      │   kv_server      │◄────── kv_bench (load testing,
   product_api ──────►│  (C++, TCP,      │         many concurrent clients)
   (cache-aside)       │   sharded cache) │
                     └──────────────────┘

   product_api also talks to:
                     ┌──────────────────┐
                     │  products.db      │  (SQLite — "the slow thing")
                     └──────────────────┘
```

- `kv_server` is the star of the project — all the systems engineering lives here.
- `product_api` is a deliberately thin layer (few dozen lines) whose only job is to
  give the cache a realistic reason to exist and produce a real before/after number.
- `kv_cli` and `kv_bench` are both clients of `kv_server`, used for manual demo and
  automated load testing respectively.

### Text protocol (custom, simple, newline-delimited)
```
SET <key> <value> [EX <seconds>]\r\n   -> +OK\r\n
GET <key>\r\n                          -> $<value>\r\n   or   $-1\r\n  (miss)
DEL <key>\r\n                          -> :1\r\n  or  :0\r\n
STATS\r\n                              -> +hits=.. misses=.. evictions=..\r\n
```
Loosely inspired by Redis's RESP so it's recognizable, but simplified to plain
space-delimited text for lower implementation effort.

### Sharding
- `shard_index = hash(key) % NUM_SHARDS` (e.g. `std::hash<std::string>`)
- Each shard is fully independent: its own `std::shared_mutex`, its own hashmap,
  its own LRU linked list, its own size cap. No cross-shard locking ever required
  for a single-key operation.

---

## 3. File structure

```
kv-cache-project/
├── CMakeLists.txt
├── README.md
├── cache/                        # the core C++ project
│   ├── include/
│   │   ├── shard.h
│   │   ├── kv_cache.h
│   │   ├── lru_list.h
│   │   ├── protocol.h
│   │   ├── tcp_server.h
│   │   ├── ttl_sweeper.h
│   │   ├── stats.h
│   │   └── logger.h
│   └── src/
│       ├── main.cpp
│       ├── shard.cpp
│       ├── kv_cache.cpp
│       ├── lru_list.cpp
│       ├── protocol.cpp
│       ├── tcp_server.cpp
│       ├── ttl_sweeper.cpp
│       ├── stats.cpp
│       └── logger.cpp
├── client/
│   └── kv_cli.cpp                # manual interactive client
├── bench/
│   └── kv_bench.cpp              # concurrent load-testing tool
└── demo_api/
    ├── requirements.txt
    ├── seed_db.py                # generates products.db with fake data
    ├── product_api.py            # thin REST API demonstrating cache-aside
    └── demo_cold_vs_warm.py      # script that prints the before/after latency numbers
```

Total: **~20 files** (16 in the C++ core + client + bench, 4 in the small Python
demo layer). The C++ side is where all the resume-relevant engineering lives; the
Python side is intentionally tiny and disposable.

---

## 4a. Rate limiter design (stretch goal, built on the cache itself)

Implemented entirely using primitives you already have — no new data structures in
`kv_server` needed, just a layer in `product_api.py` (or a small C++ middleware if
you want it inside `tcp_server.cpp` instead).

**Fixed-window approach (recommended for time budget):**
```
key = "ratelimit:<client_ip>:<current_unix_second>"

On each incoming request:
    count = GET key
    if count is missing:
        SET key 1 EX 1        # starts a fresh 1-second window
        allow request
    elif count < threshold (e.g. 100):
        SET key (count+1) EX 1
        allow request
    else:
        reject with 429 Too Many Requests
```
This reuses `SET ... EX` and `GET` exactly as built — no new cache-server code
required, only client-side logic in whichever layer sits in front of requests
(`product_api.py` is the natural place, since it's already the thing receiving
HTTP requests).

**Why this is worth the resume line:** it demonstrates the cache being used for a
second, genuinely different real-world purpose beyond "speed up reads" — the same
TTL mechanism now backs a lightweight DDoS-mitigation control, which is a legitimate
network-security concept (fixed-window rate limiting) that comes up in both backend
and security-adjacent interviews.

**If time allows:** implement a token-bucket variant instead, which smooths bursts
better than fixed-window (fixed-window allows up to 2x threshold requests right at a
window boundary). Worth knowing this tradeoff and being able to explain it even if
you ship the simpler fixed-window version.

---

## 4. File-by-file responsibilities

### `cache/include/lru_list.h` + `src/lru_list.cpp`
Intrusive doubly linked list used inside each shard to track recency order.
```cpp
struct LRUNode {
    std::string key;
    LRUNode* prev = nullptr;
    LRUNode* next = nullptr;
};

class LRUList {
public:
    LRUList();
    ~LRUList();
    LRUNode* pushFront(const std::string& key);  // newly inserted/accessed key
    void moveToFront(LRUNode* node);              // on access (GET/SET hit)
    LRUNode* popBack();                            // evict least-recently-used
    void remove(LRUNode* node);                    // explicit delete
private:
    LRUNode* head_;  // sentinel
    LRUNode* tail_;  // sentinel
};
```
Not thread-safe on its own — always used while the owning shard's lock is held.

### `cache/include/shard.h` + `src/shard.cpp`
One independent slice of the cache. Owns its own lock, hashmap, and LRU list.
```cpp
struct CacheEntry {
    std::string value;
    LRUNode* lruNode;               // pointer into this shard's LRUList
    std::chrono::steady_clock::time_point expiresAt; // time_point::max() = no TTL
};

class Shard {
public:
    explicit Shard(size_t maxEntries);

    bool get(const std::string& key, std::string& outValue);
    void set(const std::string& key, const std::string& value,
              std::optional<int> ttlSeconds);
    bool del(const std::string& key);
    size_t sweepExpired();           // called by TtlSweeper; returns count removed

    // stats accessors
    size_t size() const;

private:
    mutable std::shared_mutex mutex_;
    size_t maxEntries_;
    std::unordered_map<std::string, CacheEntry> map_;
    LRUList lruList_;

    void evictIfFull();              // called under write lock, from set()
    bool isExpired(const CacheEntry& entry) const;
};
```
Key correctness points to build in deliberately:
- `get()` takes a **shared (read) lock** first to check existence; if the key is
  expired, it must re-acquire an **exclusive lock** to remove it (can't upgrade a
  shared lock directly in `std::shared_mutex` — either take exclusive lock upfront
  for simplicity, or drop-and-reacquire).
- `set()` and `del()` always take the exclusive lock.
- Every successful `get()`/`set()` on an existing key calls `lruList_.moveToFront()`
  to mark it as recently used.
- `evictIfFull()` runs inside `set()`, under the write lock already held — evicts via
  `lruList_.popBack()` until under the size cap.

### `cache/include/kv_cache.h` + `src/kv_cache.cpp`
The top-level cache object. Owns N shards, routes each key to the correct shard via
hashing, and exposes a single unified interface to the rest of the app.
```cpp
class KVCache {
public:
    KVCache(size_t numShards, size_t maxEntriesPerShard);

    bool get(const std::string& key, std::string& outValue);
    void set(const std::string& key, const std::string& value,
              std::optional<int> ttlSeconds = std::nullopt);
    bool del(const std::string& key);

    Stats getStats() const;          // aggregated across all shards
    void sweepExpiredAll();          // calls sweepExpired() on every shard

private:
    std::vector<std::unique_ptr<Shard>> shards_;
    size_t shardFor(const std::string& key) const; // std::hash<string> % numShards
    mutable Stats stats_;             // atomic counters, see stats.h
};
```

### `cache/include/stats.h` + `src/stats.cpp`
Simple atomic counters for hits/misses/evictions, aggregated and exposed via the
`STATS` command. Useful both for the demo and for explaining cache behavior in an
interview.
```cpp
struct Stats {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> evictions{0};
    std::atomic<uint64_t> expirations{0};
    std::string format() const; // "hits=123 misses=45 evictions=6 expirations=2"
};
```

### `cache/include/protocol.h` + `src/protocol.cpp`
Parses raw incoming text lines into structured commands, and formats responses back
into wire text. Keeping this separate from `tcp_server.cpp` means you can unit-test
parsing without needing a real socket.
```cpp
enum class CommandType { GET, SET, DEL, STATS, UNKNOWN };

struct ParsedCommand {
    CommandType type;
    std::string key;
    std::string value;
    std::optional<int> ttlSeconds;
};

ParsedCommand parseCommand(const std::string& line);
std::string formatOk();
std::string formatValue(const std::string& value);
std::string formatMiss();
std::string formatInt(long long n);
std::string formatStats(const Stats& stats);
```

### `cache/include/tcp_server.h` + `src/tcp_server.cpp`
POSIX socket server. Accepts connections on a listener thread; spawns a handler
thread per connection (fine for a demo-scale project — mention thread-per-connection
vs thread-pool as a known tradeoff if asked in an interview). Each handler thread
loops: read a line, parse it, call the matching `KVCache` method, write the response,
repeat until the client disconnects.
```cpp
class TcpServer {
public:
    TcpServer(int port, KVCache& cache);
    void start();
    void stop();
private:
    void acceptLoop();
    void handleClient(int clientFd);
    int port_;
    int listenFd_;
    KVCache& cache_;
    std::thread acceptThread_;
    std::atomic<bool> running_;
    std::vector<std::thread> clientThreads_; // joined/cleaned up on stop()
};
```

### `cache/include/ttl_sweeper.h` + `src/ttl_sweeper.cpp`
Background thread that periodically calls `cache.sweepExpiredAll()` so keys nobody
reads again still eventually get reclaimed.
```cpp
class TtlSweeper {
public:
    TtlSweeper(KVCache& cache, int intervalMs = 1000);
    void start();
    void stop();
private:
    void run();
    KVCache& cache_;
    int intervalMs_;
    std::atomic<bool> running_;
    std::thread thread_;
};
```

### `cache/include/logger.h` + `src/logger.cpp`
Timestamped structured logging to stdout — connection events, eviction events
(useful to print when it happens, so the LRU demo is visibly provable), server
startup config.

### `cache/src/main.cpp`
Parses CLI flags (`--port=6380 --shards=16 --max-entries-per-shard=1000
--sweep-interval-ms=1000`), constructs `KVCache`, `TcpServer`, `TtlSweeper`, starts
them, blocks main thread until shutdown (e.g. Ctrl+C via a signal handler).

### `client/kv_cli.cpp`
Small interactive/one-shot CLI client:
```
./kv_cli --target=localhost:6380 SET name Aprajita
./kv_cli --target=localhost:6380 GET name
./kv_cli --target=localhost:6380 STATS
```
Opens a TCP connection, sends one line, prints the response, exits (or stays
interactive if run with no trailing command — a REPL is a nice touch for live demos).

### `bench/kv_bench.cpp`
The benchmark tool that produces your headline numbers.
```
./kv_bench --target=localhost:6380 --clients=50 --duration=10 --read-write-ratio=90:10 --distribution=zipfian --zipfian-theta=0.99
```
Spawns `--clients` threads, each opening its own connection and hammering
GET/SET (in the given ratio) for `--duration` seconds.

**Key distribution: implement a Zipfian generator, not just uniform.** Real-world
traffic is heavily skewed — a small number of "hot" keys get accessed far more often
than the rest (think: a handful of trending products, not a flat spread across the
catalog). A uniform key distribution spreads load evenly across all shards by
construction, which makes sharding look artificially good and doesn't actually test
the thing that matters: what happens when many concurrent requests land on the
*same* shard because they're all hitting the same hot key(s). Implement a standard
Zipfian generator (rank-based, tunable skew parameter theta — 0.99 is a common
default that approximates real cache workloads, e.g. YCSB's default) and support both
`--distribution=uniform` and `--distribution=zipfian` so you can run and compare
both.

This distinction is also a genuinely good interview talking point: with Zipfian
traffic, your speedup from sharding will be noticeably lower than under uniform
traffic, because hot keys still serialize on a single shard's lock no matter how
many shards you have. Being able to explain *why* that happens (sharding distributes
key-space, not request-volume-per-key) and what you'd do about it (hot-key detection
with a small thread-local/lock-free cache in front, or consistent hashing with
virtual nodes to spread a hot key's neighbors more evenly) shows real understanding,
not just a benchmark number.

Track per-request latency (store in a vector or histogram), then print:
```
Total ops: 8,421,130
Throughput: 842,113 ops/sec
Latency p50: 0.09ms  p95: 0.22ms  p99: 0.31ms
```
Run this at `--shards=1` vs `--shards=16`, and `--distribution=uniform` vs
`--distribution=zipfian`, to produce your core comparison numbers — four
combinations total, worth capturing all of them for the write-up even if you only
headline one or two.

### `demo_api/seed_db.py`
Creates `products.db` (SQLite) with a `products` table and inserts ~10,000 rows of
fake data (id, name, price, stock) using simple randomized generation. Run once
before the demo.

### `demo_api/product_api.py`
Minimal Flask (or FastAPI) app, a few dozen lines, implementing the cache-aside
pattern:
```python
GET /product/<id>
    1. Ask kv_server: GET product:<id>
    2. If hit -> return immediately, log "CACHE HIT"
    3. If miss -> query SQLite (SELECT * FROM products WHERE id=?),
                  log "CACHE MISS - DB query took Xms",
                  write result back with SET product:<id> <json> EX 60,
                  return it
```
**Critical implementation detail — do not open a new socket per request.** If this
script opens and closes a raw TCP connection to `kv_server` on every HTTP request,
the benchmark numbers will be dominated by Python's socket setup/teardown and TCP
handshake cost, not by the cache's actual lookup speed — this would silently mask
the exact thing the project is trying to prove. Instead:
- Open **one persistent socket** to `kv_server` at Flask startup (module-level global,
  reused across requests), since Flask's dev server is single-threaded by default.
- If running multi-threaded/multi-worker (e.g. via gunicorn with multiple workers),
  use a small connection pool (one persistent socket per worker/thread) instead of a
  single shared socket, to avoid interleaving reads/writes from different requests
  on the same connection.
- Reconnect automatically if the persistent socket is ever found closed/broken.

Talks to `kv_server` over the same TCP text protocol using this persistent socket
(or reuse `kv_cli`'s protocol logic translated to Python — a ~15 line socket client
is enough, just don't reopen it per request).

### `demo_api/demo_cold_vs_warm.py`
Script that hits `GET /product/1234` twice in a row and prints the timing
difference directly:
```
First request (cold):  41.2ms   [DB hit]
Second request (warm):  0.3ms   [cache hit]
Speedup: ~137x
```
This is the single number/output you'd screenshot for your resume/portfolio.

### `CMakeLists.txt`
Builds three C++ targets: `kv_server` (from `cache/src/*.cpp`), `kv_cli`
(from `client/kv_cli.cpp` + shared protocol code), `kv_bench`
(from `bench/kv_bench.cpp`). C++17, links `pthread`.

### `demo_api/requirements.txt`
```
flask
```
(or `fastapi` + `uvicorn` if preferred — either is fine, this layer is disposable)

---

## 5. Build & run instructions for the README

```bash
# Build the C++ core
mkdir build && cd build
cmake ..
make
# produces: kv_server, kv_cli, kv_bench

# Terminal 1: start the cache server
./kv_server --port=6380 --shards=16 --max-entries-per-shard=1000

# Terminal 2: manual smoke test
./kv_cli --target=localhost:6380 SET name Aprajita
./kv_cli --target=localhost:6380 GET name

# Terminal 3: seed and start the demo API (from demo_api/)
pip install -r requirements.txt
python seed_db.py
python product_api.py   # runs on localhost:5000, talks to kv_server on 6380

# Terminal 4: the cold-vs-warm demo
python demo_cold_vs_warm.py

# Terminal 5: the concurrency benchmark
./kv_bench --target=localhost:6380 --clients=50 --duration=10
```

---

## 6. Demo script (what to actually show/record)

1. **LRU eviction proof** — set `--max-entries-per-shard=3`, insert 4 keys into the
   same shard, show the oldest one is gone via `GET`, show a newer one is still
   present. This proves LRU is real, not just claimed.
2. **TTL proof** — `SET session123 token EX 5`, `GET` immediately (hit), wait 6
   seconds, `GET` again (miss) — show the key expired on its own.
3. **Cache-aside in a real app** — run `demo_cold_vs_warm.py`, show the first request
   hitting SQLite (slow) and the second hitting the cache (fast), with real numbers.
4. **Concurrency payoff** — run `kv_bench` at `--shards=1` then `--shards=16`, under
   both `--distribution=uniform` and `--distribution=zipfian`, show all four
   throughput numbers side by side. The uniform comparison is your headline number;
   the Zipfian comparison is your deeper interview talking point (smaller speedup,
   because hot keys still serialize on one shard's lock).
5. (Stretch) A small matplotlib/gnuplot graph of throughput vs. shard count
   (1, 2, 4, 8, 16, 32), showing it scale up and then plateau near your CPU's core
   count — good talking point about diminishing returns from over-sharding.
6. (Stretch) **Rate limiter demo** — fire requests at `product_api.py` faster than
   the configured threshold (e.g. a small script sending 200 req/sec from one IP)
   and show it start returning 429s once the per-second counter (tracked in the
   cache via `SET ... EX 1`) exceeds the limit, then recovering automatically once
   the window resets.

---

## 7. Numbers to capture for your resume bullet
- Sharded vs. single-lock throughput under **uniform** load (e.g. "13.7x higher
  throughput than a single-lock baseline at 50 concurrent clients")
- Sharded vs. single-lock throughput under **Zipfian** load (expect a smaller but
  still real improvement — capture it honestly, it's a better interview story than
  the uniform number alone)
- Cold (DB) vs warm (cache) latency on the product API (e.g. "reduced p50 latency
  from 41ms to 0.3ms via cache-aside pattern"), measured with the persistent-socket
  client so the number reflects the cache's real speed, not Python connection overhead
- p99 latency under concurrent load at your chosen shard count
- Rate limiter threshold and observed behavior (e.g. "sustained 429 rejection above
  100 req/sec per IP with <1ms overhead per check")

## 8. Suggested resume bullet skeleton (fill in once measured)
"Built a concurrent, sharded in-memory key-value cache in C++ over raw POSIX
sockets, using per-shard `std::shared_mutex` locking, O(1) LRU eviction, and TTL
expiry; integrated it as a cache-aside layer in front of a SQLite-backed lookup
service, reducing p50 read latency from ~40ms to <1ms, benchmarked ~14x throughput
improvement over a single-lock baseline under 50 concurrent clients (uniform load),
and implemented an IP-based rate limiter on top of the same cache primitives as a
lightweight DDoS mitigation layer."
