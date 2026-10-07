# Conversion crate snapshot

This directory contains the unchanged manifest and Rust sources from
`bucketking657/libosmocore-rs`, nested crate `libosmocore-rs`, at revision
`25c4bae36a35ca2562e56db384370363ec2a6603`. The source manifest declares MIT licensing.
`provenance.json` records the origin, revision, and SHA-256 of every copied file.

The source repository is private. Keeping this reviewed snapshot in the build
context lets CI and Docker build without sibling checkouts or access tokens.
No C reference checkout or reference library is included.

Make conversion API changes in the sibling repository. To update this snapshot,
copy the committed manifest and all Rust source files from a reviewed revision,
verify them byte-for-byte, update the hashes and revision in `provenance.json`,
and run the workbench's C fixture comparisons and UI/endpoint checks.
Keep fixtures shared byte-for-byte between the two projects. Do not edit the
vendored implementations independently.

This fixes development and container builds. Publishing gsmtap-rs to crates.io
still requires libosmocore-rs to be available in that registry.
