#!/bin/bash
echo "--- UNIFORM DISTRIBUTION ---"
for shards in 1 2 4 8 16; do
  ./build/kv_cache_microbench --clients=50 --duration=5 --read-write-ratio=90:10 --distribution=uniform --shards=$shards | grep -E "Shards|Total|Throughput"
done
echo "--- ZIPFIAN DISTRIBUTION ---"
for shards in 1 2 4 8 16; do
  ./build/kv_cache_microbench --clients=50 --duration=5 --read-write-ratio=90:10 --distribution=zipfian --zipfian-theta=0.99 --shards=$shards | grep -E "Shards|Total|Throughput"
done
