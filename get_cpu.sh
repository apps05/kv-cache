#!/bin/bash
./build/kv_server --port=6380 --shards=16 --max-entries-per-shard=1000 > server_test.log 2>&1 &
SERVER_PID=$!
sleep 1
./build/kv_bench --target=localhost:6380 --clients=50 --duration=5 --read-write-ratio=90:10 --distribution=uniform > bench_test.log 2>&1 &
BENCH_PID=$!

sleep 2
echo "Server CPU:"
ps -p $SERVER_PID -o %cpu
echo "Bench CPU:"
ps -p $BENCH_PID -o %cpu

wait $BENCH_PID
kill $SERVER_PID
