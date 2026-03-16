#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "usage: $0 <server_bin> <SIGINT|SIGTERM>" >&2
    exit 2
fi

SERVER_BIN="$1"
SIGNAL_NAME="$2"

if [[ "$SIGNAL_NAME" != "SIGINT" && "$SIGNAL_NAME" != "SIGTERM" ]]; then
    echo "unsupported signal: $SIGNAL_NAME" >&2
    exit 2
fi

pick_port() {
    echo $((20000 + RANDOM % 20000))
}

start_server() {
    local port="$1"
    "$SERVER_BIN" --port "$port" --aof "/tmp/vortek-signal-test-${port}.aof" --log-level error \
        >/tmp/vortek-signal-test-"${port}".log 2>&1 &
    PID=$!
}

wait_for_boot() {
    local pid="$1"
    for _ in $(seq 1 20); do
        if ! kill -0 "$pid" 2>/dev/null; then
            return 1
        fi
        sleep 0.1
    done
    return 0
}

PORT="$(pick_port)"
PID=""
start_server "$PORT"

if ! wait_for_boot "$PID"; then
    echo "server failed to start (pid $PID)" >&2
    wait "$PID" || true
    exit 1
fi

kill "-${SIGNAL_NAME#SIG}" "$PID"

for _ in $(seq 1 50); do
    if ! kill -0 "$PID" 2>/dev/null; then
        wait "$PID"
        exit_code=$?
        if [[ "$exit_code" -ne 0 ]]; then
            echo "server exited with non-zero code: $exit_code" >&2
            exit 1
        fi
        exit 0
    fi
    sleep 0.1
done

echo "server did not exit after ${SIGNAL_NAME}" >&2
kill -KILL "$PID" 2>/dev/null || true
wait "$PID" || true
exit 1
