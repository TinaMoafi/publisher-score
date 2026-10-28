
# Architecture, interface contract, and validation

[Overview](../README.md) · [Runbook](DOCKER_SETUP.md) · [Roadmap](DEVELOPMENT.md)

## Scope and deployment

The incoming path is **tested**: C++ CSV publisher (`publisher-node`, `172.31.0.10`) → vSomeIP event over Docker Ethernet → S-CORE `someipd` → `gatewayd` → `mw::com`/LoLa shared-memory IPC → `lidar_ipc_verifier` (`middleware-node`, `172.31.0.20`). Outbound forwarding to an external ECU is planned, not verified.

| Unit | Code or config location | Responsibility |
| --- | --- | --- |
| Publisher | `publisher/publisher/LidarCSV_SOMEIP_Publisher.cpp` | Reads CSV frame groups, serializes and offers event |
| Publisher runner | `publisher/scripts/lidar-node-publisher.sh` | Sets vSomeIP config, adds route, builds and launches publisher |
| Publisher vSomeIP config | `publisher/config/vsomeip/publisher-container.json` | Unicast and SOME/IP service offer |
| Middleware vSomeIP config | `middleware/configs/vsomeip/middleware-container.json` | Unicast and SOME/IP client config |
| S-CORE SOME/IP daemon | `middleware/inc_someip_gateway/src/someipd/` | SOME/IP networking and LoLa-facing service |
| S-CORE gateway | `middleware/inc_someip_gateway/src/gatewayd/` | SOME/IP-to-LoLa event bridge |
| Verifier | `middleware/inc_someip_gateway/examples/lidar_visualizer_sim/src/lidar_ipc_verifier.cpp` | GenericProxy receive and expected-header probe |

## Transport identifiers

| Parameter | Value | Source |
| --- | --- | --- |
| Service | `0x1234` | Both vSomeIP configs, publisher code |
| Instance | `0x5678` | Both vSomeIP configs, publisher code |
| Event / eventgroup | `0x8778` | Publisher vSomeIP config and source |
| Publisher address | `172.31.0.10` | Root Compose, publisher config |
| Middleware address | `172.31.0.20` | Root Compose, middleware config |
| SD multicast | `224.224.224.245:30490/UDP` | Both vSomeIP JSON configs |
| Service unreliable port | `30509/UDP` | Both vSomeIP JSON configs |

## Serialization contract

The authoritative publisher source defines the following record format. Header: `ray_count` (native uint32, 4 B) and `frame_timestamp` (native double, 8 B). Each ray: `ray_timestamp` (double, 8 B), `ray_id` (uint32, 4 B), `x`, `y`, `z`, and `intensity` (four floats, 16 B). A frame with 36 rays is **4 + 8 + 36×28 = 1020 bytes**. The publisher's `push_bytes<T>` copies native object representations into a byte vector; therefore, interoperability across endianness or non-IEEE floating-point targets is **not guaranteed** without explicit standardized serialization.

Observed LoLa GenericProxy sample layout in this build:

```text
Offset 0x00: 4 bytes  payload size = 1020 (fc 03 00 00)
Offset 0x04: 12 bytes reserved/prefix, zero in observed test
Offset 0x10: 4 bytes  ray_count = 36 (24 00 00 00)
Offset 0x14: 8 bytes  frame timestamp
Offset 0x1c: ...      per-ray serialized records
```

The verifier reports a LoLa sample **storage slot of 65584 bytes**, not a 65584-byte application message. The 16-byte prefix is observed in this generic-event representation and is not a universal LoLa on-wire specification.

## Validation protocol

1. Prepare the middleware environment, including Docker networking, multicast routing, the manual LoLa dependency corrections, and the required Bazel builds. Follow [Setup and operation](DOCKER_SETUP.md) for the complete procedure.
2. Start `gatewayd`, followed by `someipd`, `lidar_ipc_verifier`, and finally the LiDAR publisher.
3. Verify that `someipd` enters its routing and network processing loops.
4. Confirm that the IPC verifier discovers the configured service and subscribes to `lidar_frame`.
5. Confirm that the gateway receives SOME/IP events with `event=0x8778` and `payload=1020B` and reports forwarding them to IPC subscribers.
6. Confirm that the verifier recognizes `ray_count=36` and reports `PASS` at payload offset 16.
7. Record the dependency version, Git commit, Docker configuration, and representative application logs for reproducibility.

### Validation result

The following output demonstrates successful structural reception of a LiDAR frame through LoLa IPC:

```text
[ipc-verifier] ===== frame 28 =====
[ipc-verifier] candidate LiDAR payload offset=16
[ipc-verifier] candidate ray_count=36
[ipc-verifier] candidate frame_timestamp=2.8001999999999998
[ipc-verifier] PASS: found possible 1020-byte LiDAR payload at offset 16
```

| Test | Status | Evidence / remaining work |
| --- | --- | --- |
| SOME/IP publisher to gateway | Passed | Gateway reports reception of 1020-byte LiDAR events |
| Gateway to LoLa IPC verifier | Passed — structural | Expected ray count, timestamp and payload offset recognized |
| Full payload integrity | Not yet verified | Requires byte-for-byte comparison of all payload fields |
| Frame loss and ordering | Not yet measured | Requires sequence tracking and frame-count comparison |
| Outbound LoLa → SOME/IP forwarding | Planned | Requires local IPC publisher and outbound gateway configuration |
| External SOME/IP subscriber | Planned | Requires subscriber implementation and reception validation |

### Validation limitations

The current implementation verifies the successful reception and structural recognition of the expected LiDAR application payload through the inbound SOME/IP–LoLa communication path.

It does not establish byte-for-byte identity, zero frame loss, latency performance, or outbound communication.

For complete data-integrity validation, capture the 1020 application-payload bytes at both the publisher and IPC receiver. Correlate frames using timestamps or explicit sequence numbers, compare their contents, and identify missing or reordered frames.

SOME/IP transport headers and the observed 16-byte LoLa sample prefix must be excluded from application-payload comparisons.
