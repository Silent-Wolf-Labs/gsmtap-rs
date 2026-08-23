# GSMTAP workbench mode use cases

The workbench receives GSMTAP UDP datagrams on its listen endpoint. Depending
on the selected mode, it either observes the datagrams, forwards them
unchanged, or makes an explicitly edited copy available for forwarding.

![Listen, relay, and modify mode relationships](mode_use_cases.svg)

All examples below use the workbench container built by the default Dockerfile
target. GSMTAP uses UDP port `4729`; the web UI uses TCP port `8080`.

## Listen mode: inspect one source

**Use case:** A radio simulator or protocol tool sends packets to the
workbench, and an engineer watches and filters them in the web UI without
affecting the source or any downstream system.

```text
GSMTAP source  ── UDP datagrams ──>  gsmtap-workbench
                                      └─> web UI: inspect and filter
```

The workbench is passive in this mode. It binds to the listen endpoint, records
the packets in its bounded history, and does not forward them.

```bash
docker run --rm \
  -p 8080:8080 -p 4729:4729/udp \
  gsmtap-workbench \
  --mode listen \
  --gsmtap-listen 0.0.0.0:4729 \
  --http-listen 0.0.0.0:8080
```

## Relay mode: connect two endpoints

**Use case:** An engineer places the workbench between a GSMTAP producer and a
consumer to observe traffic while keeping the existing packet bytes intact.

```text
GSMTAP source  ── original datagram ──>  gsmtap-workbench
                                             ── same bytes ──> GSMTAP target
```

Relay mode forwards the received datagram to `--gsmtap-forward`. Forwarding is
payload-transparent: the workbench does not decode and re-encode the normal
relay path. The target will generally see the workbench host/container as the
UDP source address.

```bash
docker run --rm \
  -p 8080:8080 -p 4729:4729/udp \
  gsmtap-workbench \
  --mode relay \
  --gsmtap-listen 0.0.0.0:4729 \
  --gsmtap-forward 192.168.1.50:4729 \
  --http-listen 0.0.0.0:8080
```

## Modify mode: capture, edit, and send

**Use case:** A test engineer captures a packet from one endpoint, changes a
field in the web UI, previews the difference, and explicitly sends the edited
packet to another endpoint.

```text
GSMTAP source  ── packet ──>  gsmtap-workbench  ── edited packet ──> GSMTAP target
                                  │
                                  └─ web UI: preview, confirm, and send
```

Modify mode is capture-and-replay. Incoming packets are retained for later
selection; they are not paused while a person edits them. A packet is sent to
the forward endpoint only after an explicit replay or edit-and-send action.

```bash
docker run --rm \
  -p 8080:8080 -p 4729:4729/udp \
  gsmtap-workbench \
  --mode modify \
  --gsmtap-listen 0.0.0.0:4729 \
  --gsmtap-forward 192.168.1.50:4729 \
  --http-listen 0.0.0.0:8080
```

## Dockerfile terminology

The Dockerfile contains build/runtime stages, not separate workbench modes:

| Dockerfile stage | Purpose |
| --- | --- |
| `workbench` | Runtime image containing `gsmtap-workbench` |
| `traffic-test` | Python image used to replay test vectors against a workbench |
| `default` | The default output image; currently based on `workbench` |

The operating mode is selected at runtime with `--mode listen`, `--mode relay`,
or `--mode modify` (or with the equivalent `GSMTAP_MODE` environment variable).
