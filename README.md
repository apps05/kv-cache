# Concurrent Sharded In-Memory KV Cache

A high-performance, concurrent, in-memory key-value cache server written in C++ designed to sit in front of a database and absorb read traffic. Features sharded locking, LRU eviction, and TTL expiry.

## Features
- **Thread-safe single-shard hashmap**
- **Sharding** across independent shards to minimize contention
- **LRU eviction** per shard
- **TTL expiry** via background sweeper
- **Custom text protocol** over TCP
- **Cache-aside product lookup API** using Flask and SQLite

## Build Instructions

```bash
# Build the C++ core
mkdir build && cd build
cmake ..
make
```

### Run TSAN tests
```bash
mkdir build_tsan && cd build_tsan
cmake -DUSE_TSAN=ON ..
make
./kv_server --port=6380
```

## Demo Instructions

```bash
# Terminal 1: start the cache server
./kv_server --port=6380 --shards=16 --max-entries-per-shard=1000

# Terminal 2: manual smoke test
./kv_cli --target=localhost:6380 SET name Aprajita
./kv_cli --target=localhost:6380 GET name

# Terminal 3: seed and start the demo API
cd demo_api
pip install -r requirements.txt
python seed_db.py
python product_api.py   

# Terminal 4: cold vs warm test
cd demo_api
python demo_cold_vs_warm.py

# Terminal 5: benchmarking
./kv_bench --target=localhost:6380 --clients=50 --duration=10
```
