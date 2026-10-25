#!/bin/sh
# Builds the workbench and traffic-test images once, then checks each mode by
# replaying the GSMTAP vectors from the traffic container against a workbench
# on a private Podman network. Pass modes as arguments to run a subset; the
# default is listen, relay and modify. Every mode runs even if one fails.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
workbench_image=${WORKBENCH_IMAGE:-localhost/gsmtap-workbench:integration}
traffic_image=${TRAFFIC_IMAGE:-localhost/gsmtap-traffic-test:integration}
modes=${*:-listen relay modify}
run=gsmtap-integration-$$

buildah build --layers --target workbench --tag "$workbench_image" "$root"
buildah build --layers --target traffic-test --tag "$traffic_image" "$root"

cleanup() {
    podman rm --force --time 0 "$run-workbench" > /dev/null 2>&1 || true
    podman network rm --force "$run" > /dev/null 2>&1 || true
}
trap cleanup EXIT
podman network create "$run" > /dev/null

failed=
for mode in $modes; do
    echo "integration: $mode"
    podman run --detach --name "$run-workbench" \
        --network "$run" --network-alias workbench \
        --env GSMTAP_MODE="$mode" --env GSMTAP_FORWARD=traffic:9000 \
        "$workbench_image" > /dev/null

    ready=
    for _ in $(seq 50); do
        if podman exec "$run-workbench" \
            wget -q -O /dev/null http://127.0.0.1:8080/api/status 2> /dev/null; then
            ready=1
            break
        fi
        sleep 0.2
    done

    if [ -n "$ready" ] && podman run --rm \
        --network "$run" --network-alias traffic \
        "$traffic_image" \
        --mode "$mode" --workbench-host workbench --sink-port 9000; then
        echo "integration: $mode passed"
    else
        echo "integration: $mode failed; workbench log:" >&2
        podman logs "$run-workbench" >&2 || true
        failed="$failed $mode"
    fi
    podman rm --force --time 0 "$run-workbench" > /dev/null
done

if [ -n "$failed" ]; then
    echo "integration: failed:$failed" >&2
    exit 1
fi
