# LiDAR Communication Gateway for Eclipse S-CORE

A reproducible integration environment for transporting LiDAR frames between external SOME/IP endpoints and applications using Eclipse S-CORE `mw::com` / LoLa inter-process communication (IPC).

The project uses a simulated LiDAR publisher, a SOME/IP-to-IPC gateway, and a LoLa subscriber to exercise the communication boundary between an external ECU or sensor and S-CORE. The architecture also accommodates the reverse IPC-to-SOME/IP direction, enabling a received frame to be forwarded to an external ECU or test receiver. The applications can simply relay frames; no perception or vehicle-control algorithms are required to exercise the communication paths.

## System architecture

```text
External LiDAR Publisher
          |
          | SOME/IP over Ethernet
          v
  +-----------------------------------------------+
  |                 Eclipse S-CORE                |
  |                                               |
  |  someipd                                      |
  |     |                                         |
  |  gatewayd (SOME/IP -> IPC)                    |
  |     |                                         |
  |     | mw::com / LoLa                           |
  |     v                                         |
  |  LiDAR Subscriber / Forwarder                 |
  |     |                                         |
  |     | mw::com / LoLa                           |
  |     v                                         |
  |  gatewayd (IPC -> SOME/IP)                    |
  |     |                                         |
  |  someipd                                      |
  +-----------------------------------------------+
          |
          | SOME/IP over Ethernet
          v
  External SOME/IP Receiver / ECU
```

The diagram represents the **intended full communication topology**, not a claim that every segment has been implemented. The incoming path (external SOME/IP → `someipd` → `gatewayd` → LoLa subscriber) has been exercised successfully. The outgoing forwarding path is part of the project architecture and requires its corresponding publisher, gateway configuration, and external receiver to be implemented and validated.

## Components

| Component | Responsibility |
| --- | --- |
| External LiDAR publisher | Reads simulated LiDAR data, serializes a frame, and publishes it over SOME/IP. |
| `someipd` | Handles the external SOME/IP communication interface. |
| `gatewayd` | Bridges SOME/IP events and S-CORE `mw::com` events. |
| LoLa / `mw::com` | Delivers events between processes using shared-memory IPC. |
| LiDAR subscriber / IPC verifier | Subscribes to LoLa samples and checks the received LiDAR frame representation. |
| LiDAR forwarder | Application role that receives a LoLa sample and republishes it for outbound delivery; it can be incorporated into the existing subscriber. |
| External receiver | Test endpoint or ECU that receives outbound SOME/IP events and validates the forwarded payload. |

## LiDAR message format

The test dataset uses **36 LiDAR rays per frame** and a **1020-byte serialized application payload**. The incoming SOME/IP event is configured as follows:

| Parameter | Value |
| --- | --- |
| Service ID | `0x1234` |
| Instance ID | `0x5678` |
| Event ID | `0x8778` |
| SOME/IP application payload length | 1020 bytes |
| LiDAR ray count | 36 |

The serialized application payload begins with a 32-bit ray count and a 64-bit floating-point frame timestamp. Its per-ray layout is defined by the publisher's serialization implementation; use that implementation as the authoritative reference for field types and byte offsets.

In the generic LoLa representation observed by the IPC verifier, the sample includes a 16-byte prefix before the serialized application payload:

```text
LoLa sample offset   Content
------------------   -----------------------------------------
0x00 - 0x03          Serialized payload length: 1020 (uint32)
0x04 - 0x0F          Prefix / reserved bytes (zero in test)
0x10 - 0x13          LiDAR ray count: 36 (uint32)
0x14 - 0x1B          Frame timestamp (double)
0x1C onward          Serialized ray data
```

This is the **observed generic sample layout**, not a universal LoLa wire-format specification. The application payload is 1020 bytes; the LoLa sample-storage allocation can be larger. Do not confuse the two sizes.

## Development environment

The integration has been exercised using two Docker containers:

| Role | Container | Working directory | Example Docker-network IP |
| --- | --- | --- | --- |
| SOME/IP publisher | `publisher-node` | `/app/publisher-node` | `172.31.0.10` |
| S-CORE middleware | `middleware-node` | `/app/inc_someip_gateway` | `172.31.0.20` |

Prerequisites are a working Docker/Compose environment for the repository, its Bazel toolchain and external dependencies, and the project's SOME/IP and `mw::com` configuration files. The example IPs belong to the tested Docker network and should not be treated as universal defaults.

Start the repository's configured containers before running the processes below. Use separate terminals; the first three commands run **inside the middleware container**, while the publisher command runs **inside the publisher container**.

## Build and run

### 1. Build and start the gateway

```bash
cd /app/inc_someip_gateway
bazel build //src/gatewayd:gatewayd_example
bazel run //src/gatewayd:gatewayd_example
```

### 2. Start the SOME/IP daemon

```bash
cd /app/inc_someip_gateway
unset LD_LIBRARY_PATH
bazel run //src/someipd:someipd_example
```

`LD_LIBRARY_PATH` is cleared here to avoid the library-selection issue encountered in the tested container setup; other environments may not require this adjustment.

### 3. Start the LoLa IPC verifier

```bash
cd /app/inc_someip_gateway
bazel run //examples/lidar_visualizer_sim:lidar_ipc_verifier
```

The verifier uses the service-instance configuration at:

```text
examples/lidar_visualizer_sim/config/mw_com_config.json
```

### 4. Start the external LiDAR publisher

```bash
cd /app/publisher-node
bash scripts/lidar-node-publisher.sh
```

Start the publisher after the receiving components so its events can be observed. The publisher may exit after sending its configured dataset.

## Verify communication

The gateway should log reception of the configured event and its application-payload length:

```text
[gatewayd] Received SOME/IP event: event=0x8778 payload=1020B
```

The IPC verifier should locate the application payload within the LoLa sample and decode its header:

```text
[ipc-verifier] candidate LiDAR payload offset=16
[ipc-verifier] candidate ray_count=36
[ipc-verifier] candidate frame_timestamp=...
[ipc-verifier] PASS: found possible 1020-byte LiDAR payload at offset 16
```

The timestamp varies by frame. This `PASS` checks the presence and interpretation of the expected payload header; it is **not** a complete comparison of all 1020 bytes or a full validation of every ray record.

### Communication test matrix

| Communication path | Acceptance criterion | Available verification |
| --- | --- | --- |
| External SOME/IP → S-CORE IPC | LoLa subscriber identifies the 1020-byte frame and decodes the 36-ray header. | Exercised successfully using the IPC verifier. |
| S-CORE IPC → external SOME/IP | External receiver obtains the republished application payload. | Requires outbound forwarding and receiver integration. |
| End-to-end relay | Compare the complete 1020-byte source payload against the external receiver's payload, frame by frame. | Requires both paths connected and a byte-level comparator. |

For an end-to-end comparison, capture the **1020-byte serialized application payload** at both endpoints, identify corresponding frames (for example using a frame sequence or timestamp), and compare their bytes. Avoid comparing SOME/IP transport headers or LoLa storage prefixes as if they were part of the application payload.

## Code organization

The paths below are relative to `/app/inc_someip_gateway`:

```text
src/
  someipd/                         SOME/IP daemon application
  gatewayd/                        SOME/IP / IPC gateway application
examples/
  lidar_visualizer_sim/
    BUILD.bazel                    Bazel build targets
    config/mw_com_config.json     mw::com service-instance configuration
    src/lidar_ipc_verifier.cpp    Generic LoLa receive-side verification
    src/lidar_frame_types.*       LiDAR-related data types / serialization code
```

The publisher is maintained under `/app/publisher-node`; its startup entry point is `scripts/lidar-node-publisher.sh`.

For development, consult the gateway service-instance configuration, event IDs, and publisher serialization before adding a new event or changing the payload layout. The outbound event may require a distinct service/event mapping from the incoming event.

## Extending the integration

The communication design supports the following implementation work without introducing additional S-CORE microservices:

1. **Subscriber and forwarder:** extend the LiDAR subscriber to publish the received application payload through a LoLa skeleton event. Preserve the original bytes when testing transport integrity.
2. **Outbound gateway binding:** configure `gatewayd` to consume the outgoing LoLa event and transmit a SOME/IP event through `someipd`.
3. **External receiver:** subscribe to the outgoing SOME/IP service/event from outside the S-CORE container and capture each 1020-byte application payload.
4. **End-to-end test:** compare the payloads at the publisher and receiver and report frame matches, missing frames, and byte mismatches.

A separate process can be added if isolation is required, but it is not necessary merely to test the IPC-to-SOME/IP path.

## Troubleshooting and reproducibility

- **No incoming events:** check container connectivity, SOME/IP service/event identifiers, publisher startup, and `someipd`/`gatewayd` logs.
- **No LoLa samples:** check the `mw::com` service-instance manifest, gateway offering, subscriber discovery, and process startup order.
- **Unexpected sample bytes:** distinguish the LoLa sample prefix from the application payload and check that the producer and consumer reference the same sample-buffer location.
- **Shared-memory startup errors:** ensure stale gateway/verifier processes are stopped before attempting a clean restart. Do not remove `/dev/shm` objects that are still mapped by running processes.
- **Dependency reproducibility:** the tested environment required corrections to generic LoLa sample-buffer pointer handling in `score_communication` (`skeleton_memory_manager.cpp` and `skeleton.cpp`). These edits were originally made under Bazel's external dependency directory; **a fresh clone is not guaranteed to contain them**. Preserve the corrected dependency as a pinned version, reproducible patch, or upstream change before distributing a turnkey build.

The technical investigation and exact pointer corrections are documented separately in [`docs/LIDAR_COMMUNICATION_ROADMAP.md`](docs/LIDAR_COMMUNICATION_ROADMAP.md). That report is developer-facing debugging history; this README is the project entry point.

## Scope

This repository setup focuses on communication transport, event forwarding, serialization boundaries, and payload integrity. It does not depend on object detection, sensor fusion, or actuation logic. Security properties (authentication, anti-spoofing, confidentiality, and IPC access control) require separate threat modeling and verification; successful payload delivery alone does not establish a secure channel.
