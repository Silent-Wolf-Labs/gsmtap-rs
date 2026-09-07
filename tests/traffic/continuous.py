#!/usr/bin/env python3
"""Continuously send valid GSMTAP UDP traffic for interactive testing."""

import argparse
import socket
import struct
import sys
import time


GSMTAP_VERSION = 2
GSMTAP_HEADER_WORDS = 4
GSMTAP_MESSAGE_TYPE = 1
GSMTAP_EXTENSION = bytes.fromhex("DE AD BE EF")
EXTENSION_EVERY = 10


def make_packet(sequence: int, payload_size: int, extension: bytes = b"") -> bytes:
    """Build a GSMTAP v2 packet with a changing frame and payload."""
    payload_size = max(payload_size, 4)
    payload = struct.pack("!I", sequence) + bytes((sequence + index) % 256 for index in range(payload_size - 4))
    header_words = GSMTAP_HEADER_WORDS + len(extension) // 4
    header = struct.pack(
        "!BBBBHbbIBBBB",
        GSMTAP_VERSION,
        header_words,
        GSMTAP_MESSAGE_TYPE,
        sequence % 8,
        sequence % 65536,
        -50,
        20,
        sequence,
        0,
        0,
        0,
        0,
    )
    return header + extension + payload


def run(args: argparse.Namespace) -> int:
    destination = (args.host, args.port)
    deadline = None if args.duration is None else time.monotonic() + args.duration
    sent = 0
    next_send = time.monotonic()

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
        try:
            while deadline is None or time.monotonic() < deadline:
                extension = GSMTAP_EXTENSION if sent % EXTENSION_EVERY == 0 else b""
                sender.sendto(make_packet(sent, args.payload_size, extension), destination)
                sent += 1
                if sent == 1 or sent % args.report_every == 0:
                    print(f"sent {sent} packets to {args.host}:{args.port}", flush=True)
                next_send += args.interval
                time.sleep(max(0, next_send - time.monotonic()))
        except KeyboardInterrupt:
            print(f"stopped after sending {sent} packets", file=sys.stderr)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1", help="workbench UDP host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=4729, help="workbench UDP port (default: 4729)")
    parser.add_argument("--interval", type=float, default=0.1, help="seconds between packets (default: 0.1)")
    parser.add_argument("--payload-size", type=int, default=16, help="payload size in bytes (default: 16)")
    parser.add_argument("--duration", type=float, help="stop after this many seconds; otherwise run until Ctrl-C")
    parser.add_argument("--report-every", type=int, default=100, help="print progress every N packets (default: 100)")
    args = parser.parse_args()
    if args.port < 1 or args.port > 65535:
        parser.error("--port must be between 1 and 65535")
    if args.interval <= 0:
        parser.error("--interval must be positive")
    if args.payload_size < 4:
        parser.error("--payload-size must be at least 4 bytes")
    if args.duration is not None and args.duration <= 0:
        parser.error("--duration must be positive")
    if args.report_every < 1:
        parser.error("--report-every must be positive")
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
