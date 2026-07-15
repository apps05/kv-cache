#!/bin/bash
for shards in 1 2 4 8 16; do
  echo "--- Shards: $shards ---"
  ./build/kv_server --port=6380 --shards=$shards --max-entries-per-shard=1000 > server_$shards.log 2>&1 &
  SERVER_PID=$!
  sleep 1
  
  ./build/kv_bench --target=localhost:6380 --clients=50 --duration=10 --read-write-ratio=90:10 --distribution=uniform > bench_$shards.log 2>&1 &
  BENCH_PID=$!
  
  # Sample CPU usage for both processes every 2 seconds, 4 times
  for i in {1..4}; do
    sleep 2
    top -pid $SERVER_PID -pid $BENCH_PID -l 1 -s 0 | grep -E "kv_server|kv_bench" >> cpu_v2_$shards.log
  done
  
  wait $BENCH_PID
  kill $SERVER_PID
  wait $SERVER_PID 2>/dev/null
  
  cat bench_$shards.log | grep -E "Throughput|Latency|ops:"
  echo "CPU Utilisation (SERVER | BENCH):"
  cat cpu_v2_$shards.log | awk '{print $2, $3"%"}'
  echo ""
done
