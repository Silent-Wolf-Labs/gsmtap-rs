FROM rust:1.85-bookworm AS builder
WORKDIR /build
COPY Cargo.toml Cargo.lock ./
COPY vendor ./vendor
COPY src ./src
COPY static ./static
RUN cargo build --release --locked --bin gsmtap-workbench

FROM debian:bookworm-slim AS workbench
RUN apt-get update \
    && apt-get install --yes --no-install-recommends tzdata \
    && rm -rf /var/lib/apt/lists/*
COPY --from=builder /build/target/release/gsmtap-workbench /usr/local/bin/gsmtap-workbench
EXPOSE 8080/tcp 4729/udp
ENTRYPOINT ["/usr/local/bin/gsmtap-workbench"]

FROM python:3.12-alpine AS traffic-builder
WORKDIR /build
COPY tests/traffic/pyproject.toml ./
RUN pip install --no-cache-dir uv==0.8.14 \
    && uv venv /opt/venv

FROM python:3.12-alpine AS traffic-test
COPY --from=traffic-builder /opt/venv /opt/venv
COPY tests/traffic/replay.py /opt/traffic/replay.py
COPY tests/vectors /opt/traffic/vectors
ENV PATH="/opt/venv/bin:$PATH"
ENTRYPOINT ["python", "/opt/traffic/replay.py"]

# Keep the workbench image as the default output of `docker build .`.
FROM workbench AS default
