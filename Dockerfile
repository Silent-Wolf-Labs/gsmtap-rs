FROM rust:1.70-bookworm AS builder
WORKDIR /build
COPY Cargo.toml Cargo.lock ./
COPY src ./src
COPY static ./static
COPY tests ./tests
RUN cargo build --release --bin gsmtap-workbench

FROM debian:bookworm-slim
COPY --from=builder /build/target/release/gsmtap-workbench /usr/local/bin/gsmtap-workbench
EXPOSE 8080/tcp 4729/udp
ENTRYPOINT ["/usr/local/bin/gsmtap-workbench"]
