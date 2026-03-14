#!/usr/bin/env bash
# Vortek smoke test — exercises every supported command against a running server.
# Usage: ./scripts/smoke_test.sh [host] [port]
#
# Requires: redis-cli  (sudo apt install redis-tools)

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-6379}"
PASS=0
FAIL=0

# --raw: strips quotes from bulk strings and uses plain \n endings (no \r).
r() { redis-cli --raw -h "$HOST" -p "$PORT" "$@" 2>/dev/null; }

ok()   { echo "  [PASS] $1"; PASS=$((PASS + 1)); }
fail() { echo "  [FAIL] $1  →  expected='$2'  got='$3'"; FAIL=$((FAIL + 1)); }

assert_eq() {
    local desc="$1" expected="$2" got="$3"
    [[ "$got" == "$expected" ]] && ok "$desc" || fail "$desc" "$expected" "$got"
}

echo "========================================"
echo " Vortek smoke test  ($HOST:$PORT)"
echo "========================================"

# ---- PING -------------------------------------------------------------------
echo ""
echo "--- PING ---"
assert_eq "PING"         "PONG"  "$(r PING)"
assert_eq "PING message" "hello" "$(r PING hello)"

# ---- SET / GET --------------------------------------------------------------
echo ""
echo "--- SET / GET ---"
r SET smoke:str "world" > /dev/null
assert_eq "GET existing"      "world" "$(r GET smoke:str)"
assert_eq "GET missing"       ""      "$(r GET smoke:missing)"
r SET smoke:str "updated" > /dev/null
assert_eq "SET overwrites"    "updated" "$(r GET smoke:str)"

# SET NX
r DEL smoke:nx > /dev/null
r SET smoke:nx "first" NX > /dev/null
assert_eq "SET NX (absent)"   "first" "$(r GET smoke:nx)"
r SET smoke:nx "second" NX > /dev/null
assert_eq "SET NX (present)"  "first" "$(r GET smoke:nx)"

# SET XX
r DEL smoke:xx > /dev/null
r SET smoke:xx "val" XX > /dev/null
assert_eq "SET XX (absent)"   "" "$(r GET smoke:xx)"
r SET smoke:xx "initial" > /dev/null
r SET smoke:xx "replaced" XX > /dev/null
assert_eq "SET XX (present)"  "replaced" "$(r GET smoke:xx)"

# ---- DEL / EXISTS -----------------------------------------------------------
echo ""
echo "--- DEL / EXISTS ---"
r SET smoke:del "v" > /dev/null
assert_eq "EXISTS present"    "1" "$(r EXISTS smoke:del)"
assert_eq "DEL present"       "1" "$(r DEL smoke:del)"
assert_eq "EXISTS after DEL"  "0" "$(r EXISTS smoke:del)"
assert_eq "DEL missing"       "0" "$(r DEL smoke:del)"

# DEL multiple keys
r SET smoke:a "1" > /dev/null
r SET smoke:b "2" > /dev/null
assert_eq "DEL multi"         "2" "$(r DEL smoke:a smoke:b smoke:missing)"

# ---- INCR / DECR / INCRBY / DECRBY -----------------------------------------
echo ""
echo "--- INCR / DECR / INCRBY / DECRBY ---"
r DEL smoke:counter > /dev/null
assert_eq "INCR from absent"  "1"   "$(r INCR smoke:counter)"
assert_eq "INCR again"        "2"   "$(r INCR smoke:counter)"
assert_eq "DECR"              "1"   "$(r DECR smoke:counter)"
assert_eq "INCRBY 10"         "11"  "$(r INCRBY smoke:counter 10)"
assert_eq "DECRBY 3"          "8"   "$(r DECRBY smoke:counter 3)"

# ---- EXPIRE / TTL / PERSIST -------------------------------------------------
echo ""
echo "--- EXPIRE / TTL / PERSIST ---"
r SET smoke:ttl "v" > /dev/null
assert_eq "TTL no expiry"      "-1" "$(r TTL smoke:ttl)"
r EXPIRE smoke:ttl 100 > /dev/null
TTL_VAL=$(r TTL smoke:ttl)
[[ "$TTL_VAL" -gt 0 && "$TTL_VAL" -le 100 ]] \
    && ok "TTL with expiry in range" \
    || fail "TTL with expiry in range" "0<ttl<=100" "$TTL_VAL"
r PERSIST smoke:ttl > /dev/null
assert_eq "PERSIST removes TTL" "-1" "$(r TTL smoke:ttl)"
assert_eq "TTL missing key"     "-2" "$(r TTL smoke:missing)"

# ---- Key expiry (short TTL) -------------------------------------------------
echo ""
echo "--- Key expiry ---"
r SET smoke:exp "bye" EX 1 > /dev/null
assert_eq "Key alive before expiry" "bye" "$(r GET smoke:exp)"
sleep 2
assert_eq "Key gone after expiry"   ""    "$(r GET smoke:exp)"

# ---- Cleanup ----------------------------------------------------------------
r DEL smoke:str smoke:nx smoke:xx smoke:counter smoke:ttl > /dev/null 2>&1 || true

echo ""
echo "========================================"
printf " Results: %d passed, %d failed\n" "$PASS" "$FAIL"
echo "========================================"
[[ "$FAIL" -eq 0 ]]
