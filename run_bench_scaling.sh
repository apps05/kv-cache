#!/bin/bash
for shards in 1 2 4 8 16; do
  echo "--- Shards: $shards ---"
  pkill kv_server
  sleep 1
  ./build/kv_server --port=6380 --shards=$shards --max-entries-per-shard=1000 > server_$shards.log 2>&1 &
  SERVER_PID=$!
  sleep 1
  
  # Start top to monitor CPU in background
  top -pid $SERVER_PID -l 5 -s 2 | grep -A 2 'PID' > cpu_$shards.log &
  
  ./build/kv_bench --target=localhost:6380 --clients=50 --duration=10 --read-write-ratio=90:10 --distribution=uniform
  
  kill $SERVER_PID
  wait $SERVER_PID 2>/dev/null
  
  echo "CPU Utilisation (last few samples):"
  cat cpu_$shards.log
  echo ""
done
