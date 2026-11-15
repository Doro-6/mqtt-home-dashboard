FROM debian:bookworm-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential libmosquitto-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY sensor.c .
RUN cc -std=c11 -O2 -Wall -Wextra -Wpedantic -pthread sensor.c -o /sensor -lmosquitto -lm

FROM debian:bookworm-slim
RUN apt-get update \
    && apt-get install -y --no-install-recommends libmosquitto1 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --no-create-home sensor

COPY --from=build /sensor /usr/local/bin/sensor
USER sensor
ENTRYPOINT ["/usr/local/bin/sensor"]

