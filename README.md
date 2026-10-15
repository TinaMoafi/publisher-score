# LiDAR Communication Integration — Eclipse S-CORE

## Purpose

This project verifies the transport of simulated LiDAR frames from an external SOME/IP publisher into Eclipse S-CORE, through `someipd` and `gatewayd`, to a LoLa (`mw::com`) IPC subscriber. The next goal is to forward the same frame from IPC back to an external SOME/IP receiver.

**Current status:** Incoming SOME/IP → LoLa IPC verification passed. Outgoing IPC → SOME/IP and complete end-to-end payload comparison are not yet verified.

## Architecture

```text
External LiDAR Publisher
       | SOME/IP (Ethernet)
       v
     someipd
       |
     gatewayd (SOME/IP -> IPC)
       | LoLa / mw::com
       v
LiDAR IPC Verifier / Subscriber   [VERIFIED]
       | LoLa / mw::com (forwarding to be implemented)
       v
     gatewayd (IPC -> SOME/IP)    [NEXT]
       |
     someipd
       | SOME/IP (Ethernet)
       v
External LiDAR Receiver          [NEXT]
```

The same subscriber can be extended to republish the incoming sample; a separate S-CORE microservice is not required for the proposed forwarding test.

## Tested data

| Property | Value |
|---|---|
| Service ID | `0x1234` |
| Instance ID | `0x5678` |
| Incoming event ID | `0x8778` |
| Serialized LiDAR payload | 1020 bytes |
| Ray count | 36 |
| LoLa sample slot stride | 65,584 bytes |
| LoLa sample alignment | 16 bytes |
| LoLa sample slots | 10 |

In the verified generic LoLa sample, the first four bytes encode the payload length (`fc 03 00 00` = 1020, little-endian), bytes 4–15 are zero, and the LiDAR payload begins at **sample offset 16**. The payload begins with a 4-byte `ray_count` (`24 00 00 00` = 36) followed by an 8-byte `frame_timestamp` (`double`). Consult the publisher's serialization code for the complete per-ray field layout; full 36-ray decoding is not yet validated.

## Runtime environment

The previous successful test used separate Docker containers:

| Role | Container | Application directory | IP in tested setup |
|---|---|---|---|
| Publisher | `publisher-node` | `/app/publisher-node` | `172.31.0.10` |
| Middleware | `middleware-node` | `/app/inc_someip_gateway` | `172.31.0.20` |

IP addresses are configuration-specific. Start the project's Docker Compose environment before running the commands below.

## Reproduce the incoming communication test

Use separate terminals in the middleware container and start the publisher **last**. Avoid duplicate instances of the daemons and verifier.

**Terminal 1 — gatewayd**

```bash
cd /app/inc_someip_gateway
bazel build //src/gatewayd:gatewayd_example
bazel run //src/gatewayd:gatewayd_example
```

**Terminal 2 — someipd**

```bash
cd /app/inc_someip_gateway
unset LD_LIBRARY_PATH
bazel run //src/someipd:someipd_example
```

**Terminal 3 — IPC verifier**

```bash
cd /app/inc_someip_gateway
bazel run //examples/lidar_visualizer_sim:lidar_ipc_verifier
```

**Terminal 4 — publisher container**

```bash
cd /app/publisher-node
bash scripts/lidar-node-publisher.sh
```

If the processes have startup dependencies in your checkout, follow the existing deployment scripts or logs. The verifier uses `examples/lidar_visualizer_sim/config/mw_com_config.json` for its service configuration.

## Expected output

Gateway:

```text
[gatewayd] Received SOME/IP event: event=0x8778 payload=1020B
[LOLA-PRODUCER] first32: fc 03 00 00 ... 24 00 00 00 ...
```

Verifier (observed successful test):

```text
[ipc-verifier] candidate LiDAR payload offset=16
[ipc-verifier] candidate ray_count=36
[ipc-verifier] candidate frame_timestamp=9.2099899999999995
[ipc-verifier] PASS: found possible 1020-byte LiDAR payload at offset 16
```

The timestamp is an example, not a fixed expected value. `PASS` demonstrates that the verifier found the expected payload header and decoded the timestamp; it does **not** establish byte-for-byte equality for all 1020 bytes or validate all 36 ray records.

## Troubleshooting

- **Verifier reads unexpected bytes:** inspect producer/consumer slot addresses and the LoLa data mapping. A previous bug caused the producer to use sample storage offset 296 while the consumer used 368.
- **`OffsetPtr` bounds-check error:** check for stale processes and shared-memory mappings before attempting a clean restart. Do not delete a shared-memory file still mapped by a process.
- **`someipd` library/ABI problem:** the tested setup used `unset LD_LIBRARY_PATH` before starting it.
- **Missing Bazel target or configuration:** confirm your checkout includes the verifier target and its `mw_com_config.json`.

## Source changes and reproducibility warning

Two LoLa fixes were made in Bazel's `external/score_communication+` source tree. **Do not assume these fixes were included in the main repository commit.** See [technical roadmap](docs/LIDAR_COMMUNICATION_ROADMAP.md) for exact edits and a plan to preserve them as a patch or upstream change. Verify the dependency version and apply the fixes in a reproducible way before claiming a clean checkout works.

## Next milestone

Extend the existing IPC subscriber so it can publish the same received LiDAR payload on an outgoing LoLa event. Configure `gatewayd` and `someipd` for IPC → SOME/IP, implement an external SOME/IP test receiver, and compare all 1020 bytes with the publisher's original frame.

See [LiDAR communication roadmap](docs/LIDAR_COMMUNICATION_ROADMAP.md).
