FROM docker.io/library/rust:1.99-alpine AS builder
RUN apk add --no-cache musl-dev \
    && cargo install cargo-auditable --version 0.7.7 --locked
WORKDIR /build/gsmtap-rs
COPY Cargo.toml Cargo.lock ./
COPY src ./src
COPY static ./static
# The binary embeds its dependency list, so image scanners can audit it.
RUN --mount=type=cache,target=/usr/local/cargo/registry \
    --mount=type=cache,target=/build/gsmtap-rs/target \
    cargo auditable build --release --locked --bin gsmtap-workbench \
    && cp target/release/gsmtap-workbench /usr/local/bin/

FROM docker.io/library/python:3.12-alpine AS traffic-test
COPY tests/traffic/replay.py /opt/traffic/replay.py
COPY tests/vectors/*.json /opt/traffic/vectors/
ENTRYPOINT ["python", "/opt/traffic/replay.py"]

# Last, so the workbench is the default build target.
FROM docker.io/library/alpine:3.22 AS workbench
COPY --from=builder /usr/local/bin/gsmtap-workbench /usr/local/bin/gsmtap-workbench
COPY tests/vectors/*.json /usr/share/gsmtap-rs/vectors/
USER 65534:65534
ENV GSMTAP_LISTEN=0.0.0.0:4729 HTTP_LISTEN=0.0.0.0:8080 \
    GSMTAP_REFERENCE_VECTORS=/usr/share/gsmtap-rs/vectors
EXPOSE 8080/tcp 4729/udp
ENTRYPOINT ["/usr/local/bin/gsmtap-workbench"]
