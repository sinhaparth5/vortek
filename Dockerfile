FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel && \
    cmake --install build --prefix /opt/vortek

FROM debian:bookworm-slim

RUN useradd -m -u 10001 vortek
COPY --from=builder /opt/vortek /opt/vortek

USER vortek
EXPOSE 6379

ENTRYPOINT ["/opt/vortek/bin/vortek"]
