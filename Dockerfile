# Empty unless scripts/docker-build-osmocore.sh supplies the osmocore-rs crate
# as a named build context.
FROM scratch AS osmocore-rs

FROM rust:1.99-bookworm AS builder
# Set to `osmocore` (with the osmocore-rs context) for the conversion panels.
ARG FEATURES=""
WORKDIR /build/gsmtap-rs
COPY Cargo.toml Cargo.lock build.rs ./
COPY scripts/without-osmocore.sh ./scripts/
COPY src ./src
COPY static ./static
COPY --from=osmocore-rs / /build/libosmocore-rs/osmocore-rs/
RUN case ",$FEATURES," in \
        *,osmocore,*) ;; \
        *) ./scripts/without-osmocore.sh ;; \
    esac \
    && cargo build --release --locked --features "$FEATURES" --bin gsmtap-workbench

FROM debian:bookworm-slim AS workbench
RUN apt-get update \
    && apt-get install --yes --no-install-recommends tzdata \
    && rm -rf /var/lib/apt/lists/*
COPY --from=builder /build/gsmtap-rs/target/release/gsmtap-workbench /usr/local/bin/gsmtap-workbench
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
