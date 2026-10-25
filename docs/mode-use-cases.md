# GSMTAP workbench mode use cases

The workbench receives GSMTAP UDP datagrams on its listen endpoint. Depending
on the selected mode, it observes the datagrams, makes selected batches
available for byte-preserving forwarding, or makes an explicitly edited copy
available for forwarding.

![Listen, relay, and modify mode relationships](mode_use_cases.svg)

Build and run the workbench once. It starts in Listen mode; select Relay or
Modify, and configure its `host:port` forward target, from the status card in
the web UI. GSMTAP uses UDP port `4729`; the web UI uses TCP port `8080`.

```bash
buildah build --layers --tag gsmtap-workbench .
podman run --rm \
  -p 8080:8080 -p 4729:4729/udp \
  gsmtap-workbench
```

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

## Relay mode: inspect and forward selected batches

**Use case:** An engineer captures traffic from a GSMTAP producer, inspects it
in the web UI, and forwards a selected batch to a consumer while keeping the
existing packet bytes intact.

```text
GSMTAP source  ── original datagram ──>  gsmtap-workbench  ── selected batch ──> GSMTAP target
                                             ▲
                                             └─ web UI: select and Forward selected
```

Relay mode records incoming datagrams and does not forward them automatically.
Select one or more records in the web UI and use **Forward selected** to send a
batch to the forward target configured in the status card. Forwarding is
payload-transparent: the workbench does not decode and re-encode the selected
datagrams. The target will generally see the workbench host/container as the
UDP source address.

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

## Containerfile stages

The Containerfile contains build and runtime stages, not separate workbench
modes:

| Stage | Purpose |
| --- | --- |
| `builder` | Builds a static `gsmtap-workbench` with `cargo auditable` on Alpine |
| `traffic-test` | Python image used to replay test vectors against a workbench |
| `workbench` | Alpine runtime image with `gsmtap-workbench` and the reference vectors; the default target |

The operating mode is selected at runtime from the workbench status card.
