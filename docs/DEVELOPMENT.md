
# Development roadmap and contribution notes

[Overview](../README.md) · [Architecture](ARCHITECTURE_AND_TESTING.md) · [Setup](DOCKER_SETUP.md)

## Current baseline

Validated inbound transport: publisher → SOME/IP → `someipd` → `gatewayd` → LoLa → `lidar_ipc_verifier`, with 1020-byte frame structure recognized and 36 rays identified. This does not establish a byte-for-byte copy of all samples. Maintain this test as the regression baseline.

## Planned implementation

1. Extend the existing LiDAR IPC subscriber so it also offers a LoLa skeleton for forwarding, ideally without an additional microservice unless process isolation is needed.
2. Keep the application payload unchanged for initial integrity experiments. Decide the outgoing LoLa event/service/instance identity explicitly; avoid accidentally creating a loopback into the incoming path.
3. Configure `gatewayd` to bind the outgoing LoLa event to a SOME/IP publication, and `someipd` to expose the corresponding outgoing SOME/IP service/event.
4. Implement an external receiver, capture 1020-byte application payloads and pair them by timestamp or explicit sequence number.
5. Compare source and destination payload bytes, track frame loss, reordering and latency; document test fixtures and acceptance thresholds.
6. Replace the manual `score_communication` dependency workaround with a version-pinned **Bazel-applied** patch validated in a clean build; consider an upstream fix.

## Change checklist

For event/layout changes update publisher source, vSomeIP JSON files, S-CORE service-instance manifests, verifier, and architecture documentation consistently. For Docker changes update the root Compose and this runbook; avoid silently switching to the middleware-only Compose file. For dependency changes note the exact Bazel module version, patch state, build result and incoming regression test.

## Evidence to include in issues and pull requests

Host OS, Docker/Compose versions, container images, Git revision, Bazel version, `score_communication` version/patch state, reproducible commands, gateway logs and verifier output. Distinguish observed facts from hypotheses and planned behavior.
