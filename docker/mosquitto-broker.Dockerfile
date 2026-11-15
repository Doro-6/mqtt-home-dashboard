FROM debian:bookworm-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        libcjson-dev \
        libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY third_party/mosquitto/ ./

RUN cmake -S . -B /build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DWITH_TESTS=OFF \
        -DWITH_DOCS=OFF \
        -DWITH_BROKER=ON \
        -DWITH_CLIENTS=ON \
        -DWITH_APPS=OFF \
        -DWITH_PLUGINS=OFF \
        -DWITH_WEBSOCKETS=ON \
        -DWITH_WEBSOCKETS_BUILTIN=ON \
        -DWITH_TLS=ON \
        -DWITH_TLS_PSK=OFF \
        -DWITH_SQLITE=OFF \
        -DWITH_HTTP_API=OFF \
        -DWITH_CTRL_SHELL=OFF \
    && cmake --build /build --parallel \
    && cmake --install /build

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        gosu \
        libcjson1 \
        libssl3 \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --system --gid 1883 mosquitto \
    && useradd --system --uid 1883 --gid mosquitto --home-dir /var/empty --shell /usr/sbin/nologin mosquitto \
    && mkdir -p /mosquitto/config /mosquitto/data /mosquitto/log \
    && chown -R mosquitto:mosquitto /mosquitto

COPY --from=build /usr/local/ /usr/local/
COPY docker/mosquitto-entrypoint.sh /usr/local/bin/mosquitto-entrypoint

RUN chmod 755 /usr/local/bin/mosquitto-entrypoint && ldconfig

VOLUME ["/mosquitto/data", "/mosquitto/log"]
EXPOSE 1883 9001
ENTRYPOINT ["/usr/local/bin/mosquitto-entrypoint"]
CMD ["/usr/local/sbin/mosquitto", "-c", "/mosquitto/config/mosquitto.conf"]
