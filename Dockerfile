FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        ca-certificates \
        cmake \
        g++ \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /source

COPY CMakeLists.txt ./
COPY src ./src

RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF \
        -DCOLUMNAR_ENABLE_NATIVE_OPTIMIZATIONS=OFF \
    && cmake --build build \
        --target benchmark-service \
        --parallel

FROM ubuntu:24.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        curl \
        libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir /data \
    && chown 10001:10001 /data

COPY --from=builder \
    /source/build/benchmark-service \
    /usr/local/bin/benchmark-service

ENV BENCHMARK_HOST=0.0.0.0 \
    BENCHMARK_PORT=8080 \
    BENCHMARK_DATA_DIR=/data

USER 10001:10001

EXPOSE 8080

HEALTHCHECK --interval=10s --timeout=2s --start-period=5s --retries=3 \
    CMD curl --fail --silent --show-error --output /dev/null \
        "http://127.0.0.1:${BENCHMARK_PORT}/health"

ENTRYPOINT ["/usr/local/bin/benchmark-service"]
