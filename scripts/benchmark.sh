#!/usr/bin/env bash
# benchmark.sh — network-level throughput benchmark against a running Vortek server.
# Requires redis-benchmark (ships with redis-tools on most distros).
#
# Usage:
#   ./scripts/benchmark.sh [host] [port]
#   Default: host=127.0.0.1  port=6379
#
# Start the server first:
#   ./build/vortek --no-aof
#
# Then in another terminal:
#   ./scripts/benchmark.sh

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-6379}"
CLIENTS=50
REQUESTS=100000
PIPELINE=16

echo "========================================"
echo "  Vortek Network Benchmark"
echo "  host=$HOST  port=$PORT"
echo "  clients=$CLIENTS  requests=$REQUESTS  pipeline=$PIPELINE"
echo "========================================"

if ! command -v redis-benchmark &>/dev/null; then
    echo "redis-benchmark not found — install redis-tools:"
    echo "  sudo apt install redis-tools"
    exit 1
fi

# Verify the server is reachable
if ! redis-benchmark -h "$HOST" -p "$PORT" -n 1 -q PING &>/dev/null; then
    echo "ERROR: could not connect to Vortek at $HOST:$PORT"
    echo "Start the server first:  ./build/vortek --no-aof"
    exit 1
fi

run() {
    local label="$1"; shift
    echo ""
    echo "--- $label ---"
    redis-benchmark \
        -h "$HOST" -p "$PORT" \
        -c "$CLIENTS" -n "$REQUESTS" -P "$PIPELINE" \
        --csv -q "$@"
}

run "PING"         PING
run "SET"          SET bench:key bench:value
run "GET (hit)"    GET bench:key
run "INCR"         INCR bench:counter
run "LPUSH"        LPUSH bench:list element
run "RPUSH"        RPUSH bench:list element
run "SADD"         SADD bench:set member
run "HSET"         HSET bench:hash field value
run "LRANGE 0 99"  LRANGE bench:list 0 99

echo ""
echo "========================================"
echo "  Done"
echo "========================================"
