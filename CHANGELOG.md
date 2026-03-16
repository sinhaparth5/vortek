# Changelog

All notable changes to this project are documented in this file.

## [0.1.0] - 2026-03-16

### Added

- RESP2 protocol parser/serializer with command dispatch architecture.
- Core key-value store with TTL support and background expiry sweep.
- String, list, set, and hash command families.
- Async TCP server with pub/sub and AOF persistence/replay.
- CI workflows for build/test, formatting checks, and smoke testing.
- Reliability hardening: graceful signal shutdown, AOF tail recovery, connection/backpressure limits.
- Security hardening: `AUTH`, request-size limits, and idle connection timeout.
- Observability features: `HEALTH`, `READY`, `METRICS`, structured log format option.
- Compatibility coverage: command parity tests and compatibility matrix documentation.
- Release packaging support via Docker and CPack (`.tar.gz` + `.deb`).
