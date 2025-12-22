# Multi-stage Dockerfile for distributed-cache

# Builder stage
FROM ubuntu:24.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src
RUN mkdir -p cpp/build && cd cpp/build && cmake .. && cmake --build . -j$(nproc) \
    && mv distributed_cache /usr/local/bin/

# Final stage
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates && rm -rf /var/lib/apt/lists/*
COPY --from=builder /usr/local/bin/distributed_cache /usr/local/bin/distributed_cache
COPY scripts/cluster_demo.py /usr/local/bin/cluster_demo.py
RUN chmod +x /usr/local/bin/cluster_demo.py

ENTRYPOINT ["/usr/local/bin/distributed_cache"]
