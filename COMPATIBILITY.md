# Vortek Compatibility Matrix (RESP2 / Redis-style)

This matrix documents command-level compatibility targets for the current release.

## Protocol

| Area | Status | Notes |
|---|---|---|
| RESP2 parsing | Supported | Simple string, error, integer, bulk string, array, null |
| RESP2 serialization | Supported | Server replies and push messages use RESP2 |
| Pipelining | Supported | Multiple commands can be parsed from one read buffer |
| RESP3 | Not supported | RESP2 only |

## Commands

| Group | Command | Status | Notes |
|---|---|---|---|
| Generic | `PING` | Full | `PING [msg]` |
| Generic | `DEL` | Full | multi-key |
| Generic | `EXISTS` | Full | multi-key count |
| Generic | `EXPIRE` | Full | seconds |
| Generic | `TTL` | Full | `-1` no expiry, `-2` missing |
| Generic | `PERSIST` | Full | removes expiry |
| Generic | `INFO` | Partial | `server`, `clients`, `stats`, `keyspace`, `all` |
| Generic | `METRICS` | Extension | Prometheus-style text payload |
| Generic | `HEALTH` | Extension | liveness check |
| Generic | `READY` | Extension | readiness check |
| Security | `AUTH` | Basic | single password via `requirepass` |
| String | `SET` | Partial | `EX`, `PX`, `NX`, `XX`; no `GET`, `KEEPTTL` |
| String | `GET` | Full | bulk or null |
| String | `INCR`/`DECR` | Full | integer semantics, overflow checks |
| String | `INCRBY`/`DECRBY` | Full | integer semantics |
| List | `LPUSH`/`RPUSH` | Full | |
| List | `LPOP`/`RPOP` | Full | single pop |
| List | `LLEN` | Full | |
| List | `LRANGE` | Full | inclusive indices, negative indices |
| List | `LINDEX` | Full | negative indices supported |
| Set | `SADD`/`SREM` | Full | |
| Set | `SMEMBERS` | Full | unordered response |
| Set | `SISMEMBER`/`SCARD` | Full | |
| Hash | `HSET`/`HGET` | Full | |
| Hash | `HDEL`/`HGETALL` | Full | |
| Hash | `HEXISTS`/`HLEN` | Full | |
| Pub/Sub | `SUBSCRIBE`/`UNSUBSCRIBE` | Basic | push-mode behavior |
| Pub/Sub | `PUBLISH` | Basic | returns subscriber count |

## Error/Behavior Parity Notes

- Unknown commands return `ERR unknown command '<name>'`.
- Wrong arity returns `ERR wrong number of arguments for '<name>' command`.
- Cross-type operations return `WRONGTYPE Operation against a key holding the wrong kind of value`.
- Authentication-gated requests return `NOAUTH Authentication required.` when `requirepass` is set.

## Out-of-Scope (Current)

- Transactions (`MULTI`/`EXEC`), scripting, replication, clustering
- Streams, sorted sets, bit operations, scan family
- ACL users/roles (only single shared password auth)
