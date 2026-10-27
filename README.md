# LiDAR Communication Gateway for Eclipse S-CORE

Integration and validation environment for transferring simulated LiDAR frames between an external SOME/IP publisher and Eclipse S-CORE applications using `mw::com` / LoLa shared-memory IPC.

> **Current verification status (27 October 2026):** The inbound path **SOME/IP publisher → `someipd` → `gatewayd` → LoLa → IPC verifier** has been tested successfully with 1020-byte serialized LiDAR payloads containing 36 rays. The reverse forwarding path is an architectural target, **not yet validated**. A fresh clone currently requires the manual LoLa dependency corrections documented below.

## 1. Architecture and scope

```text
  External LiDAR Publisher (publisher-node)
                  |
                  | SOME/IP over Docker Ethernet
                  v
  +--------------------------------------------------+
  | Eclipse S-CORE middleware (middleware-node)       |
  |                                                  |
  |  someipd                                         |
  |     |                                            |
  |  gatewayd: incoming SOME/IP → LoLa IPC            |
  |     |                                            |
  |     | mw::com / LoLa shared memory                |
  |     v                                            |
  |  LiDAR IPC subscriber / verifier  [TESTED]        |
  |     |                                            |
  |     | Planned: same application republishes IPC   |
  |     v                                            |
  |  gatewayd: outgoing LoLa IPC → SOME/IP [PLANNED]  |
  |     |                                            |
  |  someipd [PLANNED OUTBOUND PATH]                  |
  +--------------------------------------------------+
                  |
                  | SOME/IP
                  v
  External SOME/IP Receiver / ECU [PLANNED]
```

The project tests communication, event serialization boundaries, shared-memory delivery, and eventually end-to-end payload integrity. It does not implement LiDAR perception, sensor fusion, or vehicle control. Successful delivery is not evidence of transport security.

### Components

| Component | Role | Status |
| --- | --- | --- |
| Simulated LiDAR publisher | Serializes frames and sends SOME/IP events | Tested |
| `someipd` | SOME/IP interface and service discovery | Tested inbound |
| `gatewayd` | Bridges SOME/IP and `mw::com` | Tested inbound |
| LoLa / `mw::com` | Shared-memory IPC transport | Tested inbound |
| `lidar_ipc_verifier` | Subscribes to LoLa and checks the received frame header | Tested |
| LiDAR subscriber/forwarder | Receives and republishes frames via IPC | Forwarding planned |
| External SOME/IP receiver | Captures forwarded frames | Planned |

## 2. Repository layout and environment

The commands in this guide assume the existing project Docker environment is available and running. They are **commands inside containers**, not instructions for installing Docker or building container images from scratch. Docker image creation, Compose invocation, host prerequisites, and the public clone URL are not specified in the source README and must be supplied from the actual repository setup. Do not assume that `docker compose up` works without inspecting the repository.

| Role | Docker container | Directory inside container | Example network IP |
| --- | --- | --- | --- |
| Publisher | `publisher-node` | `/app/publisher-node` | `172.31.0.10` |
| Middleware | `middleware-node` | `/app/inc_someip_gateway` | `172.31.0.20` |

These IP addresses describe the tested Docker network, not a universal requirement. Ensure both containers can communicate and the configured SOME/IP service-discovery network matches the actual environment.

Relevant paths inside `/app/inc_someip_gateway`:

```text
src/someipd/                                  SOME/IP daemon
src/gatewayd/                                 SOME/IP / IPC gateway
src/gatewayd/etc/mw_com_config.json          Gateway service-instance manifest
examples/lidar_visualizer_sim/
  BUILD.bazel                                 Verifier Bazel target
  config/mw_com_config.json                  Verifier service-instance manifest
  src/lidar_ipc_verifier.cpp                  LoLa GenericProxy verifier
  src/lidar_frame_types.*                     LiDAR serialization/data types
```

Publisher entry point inside `/app/publisher-node`:

```text
scripts/lidar-node-publisher.sh
```

### Prerequisites

- Access to the project repository, its Docker setup, and both container images.
- Docker and the project's required container networking configured on the host.
- Bazel and the S-CORE dependencies available in the middleware container.
- Publisher dependencies available in the publisher container.
- Ability to edit the Bazel-fetched `score_communication` source inside the middleware container (temporary workaround below).
- Separate terminal sessions for `someipd`, `gatewayd`, the verifier, and publisher.

If you are on a fresh machine, first clone the repository using its actual GitHub URL, follow its Docker setup instructions, and confirm both containers start. The original README does not include enough information to reconstruct the Docker images independently; obtain that information from the repository's Docker files or project maintainer.

## 3. LiDAR service and payload format

| Parameter | Tested value |
| --- | --- |
| SOME/IP service ID | `0x1234` |
| SOME/IP instance ID | `0x5678` |
| SOME/IP event ID | `0x8778` |
| Serialized application payload | **1020 bytes** |
| Rays per frame | **36** |

The publisher serializes the LiDAR frame. The authoritative field order and per-ray structure are defined in the publisher's serialization implementation; consult that code before changing the format.

The current GenericProxy verifier observes a 16-byte prefix before the serialized application payload:

```text
Offset in observed LoLa sample    Content
0x00–0x03                         Payload size: 1020 (uint32, observed little-endian)
0x04–0x0F                         12-byte prefix/reserved area (zero in test)
0x10–0x13                         Ray count: 36 (uint32, observed little-endian)
0x14–0x1B                         Frame timestamp (double)
0x1C onward                       Serialized ray records
```

Example first bytes:

```text
fc 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 24 00 00 00 ...
|--- 1020 ---|                                   |-- 36 ---|
```

**Important:** The observed LoLa storage slot size is **65584 bytes**; it is not the transmitted SOME/IP application payload size. The 16-byte prefix is an observed implementation detail, **not** a universal LoLa serialization standard.

## 4. Mandatory fresh-clone LoLa workaround

### Why this is necessary

On the tested fresh Debian setup, `gatewayd` received SOME/IP events with `payload=1020B`, but the IPC verifier could not locate `ray_count=36`. Investigation identified incorrect handling of the generic LoLa sample-buffer pointer in the Bazel-fetched `score_communication` dependency. The producer and consumer were interpreting different sample-buffer locations (an observed 72-byte displacement in the previous investigation).

The project application changes can be present in GitHub while the LoLa dependency modifications are absent: the files below live in Bazel's external dependency tree, which is **not normally tracked by the application's Git repository**. Therefore, a new clone may compile and receive SOME/IP events but still fail the IPC verifier.

**Apply the following three source corrections before building/running the middleware on a new system.** These edits are a temporary, tested workaround for this dependency version, not an upstream-approved fix. They may need reevaluation after dependency upgrades.

### 4.1 Locate the external dependency

Inside `middleware-node`:

```bash
cd /app/inc_someip_gateway
ls bazel-inc_someip_gateway/external/score_communication+/score/mw/com/impl/bindings/lola/
```

The commands below use the external dependency path observed in the tested build. If it differs, locate the corresponding `score_communication` Bazel external repository first. If the directory does not exist because Bazel has not fetched dependencies yet, build or query the relevant Bazel target to fetch them, then return to these steps. **Do not run the final application test until the corrections are applied and rebuilt.**

### 4.2 Correction A — Return the sample data buffer during creation

File:

```text
bazel-inc_someip_gateway/external/score_communication+/score/mw/com/impl/bindings/lola/skeleton_memory_manager.cpp
```

Function: `SkeletonMemoryManager::CreateGenericEventDataInCreatedSharedMemory()`.

Find the return statement near the end of the function:

```cpp
return data_storage;
```

Replace with:

```cpp
return event_data_raw_array;
```

The function already obtains `event_data_raw_array` from `data_storage->data()`. Returning the container pointer instead of the underlying sample buffer leads to incorrect sample addressing.

For the **verified version of this file**, the change was on line 265:

```bash
sed -i '265s/return data_storage;/return event_data_raw_array;/' \
  bazel-inc_someip_gateway/external/score_communication+/score/mw/com/impl/bindings/lola/skeleton_memory_manager.cpp
```

Inspect the function before using this line-number-based command on a different dependency revision.

### 4.3 Correction B — Return the sample data buffer when reopening

File:

```text
bazel-inc_someip_gateway/external/score_communication+/score/mw/com/impl/bindings/lola/skeleton.cpp
```

Function: `Skeleton::RegisterGeneric()`, in the branch that reopens existing shared memory.

Replace:

```cpp
return {static_cast<void*>(&event_data_storage), event_data_control_composite};
```

with:

```cpp
return {static_cast<void*>(event_data_storage.data()), event_data_control_composite};
```

For the **verified version**, this was line 510:

```bash
sed -i '510s/static_cast<void\*>(&event_data_storage)/static_cast<void*>(event_data_storage.data())/' \
  bazel-inc_someip_gateway/external/score_communication+/score/mw/com/impl/bindings/lola/skeleton.cpp
```

### 4.4 Correction C — Use the same storage type for creation and reopening

In `skeleton_memory_manager.cpp`, the generic storage is created as:

```cpp
storage_resource_->construct<EventDataStorage<std::max_align_t>>(...)
```

But in the tested unmodified `skeleton.cpp`, the reopened-memory branch uses:

```cpp
memory_manager_.OpenEventDataFromOpenedSharedMemory<std::uint8_t>(element_fq_id);
```

The reopening code casts the stored object to `EventDataStorage<SampleType>`, so use the same template specialization that was created:

```cpp
memory_manager_.OpenEventDataFromOpenedSharedMemory<std::max_align_t>(element_fq_id);
```

For the **verified version**, this was line 508:

```bash
sed -i '508s/OpenEventDataFromOpenedSharedMemory<std::uint8_t>/OpenEventDataFromOpenedSharedMemory<std::max_align_t>/' \
  bazel-inc_someip_gateway/external/score_communication+/score/mw/com/impl/bindings/lola/skeleton.cpp
```

### 4.5 Verify all three corrections

```bash
cd /app/inc_someip_gateway

# Must show: return event_data_raw_array;
grep -n 'return event_data_raw_array;' \
  bazel-inc_someip_gateway/external/score_communication+/score/mw/com/impl/bindings/lola/skeleton_memory_manager.cpp

# Must show both the max_align_t reopening type and the .data() return
grep -n -A 3 'OpenEventDataFromOpenedSharedMemory<std::max_align_t>' \
  bazel-inc_someip_gateway/external/score_communication+/score/mw/com/impl/bindings/lola/skeleton.cpp
```

Expected relevant lines:

```cpp
auto& event_data_storage = memory_manager_.OpenEventDataFromOpenedSharedMemory<std::max_align_t>(element_fq_id);
return {static_cast<void*>(event_data_storage.data()), event_data_control_composite};
```

Rebuild:

```bash
bazel build //src/gatewayd:gatewayd_example
```

The corrected test environment completed this build successfully. Compilation alone does not validate payload delivery; perform the runtime test in Section 6.

> **Long-term solution:** Add these changes as a version-pinned Bazel dependency patch, maintain a patched fork, or upstream the fix. A README-only workaround does **not** make a fresh clone automatically reproducible. Do not assume that edits in `bazel-*/external/` are committed when you push your application repository.

## 5. Startup order — important

**Start `someipd` before `gatewayd`.** This is not optional in the tested fresh Debian setup: `gatewayd` attempted to open `/lola-data-0000000000001337-00012` before it existed, causing `Proxy creation failed for 'lidar/LidarFrame1'`. Starting `someipd` created that LoLa instance, and restarting `gatewayd` afterward resolved the error.

Use **separate terminals** and keep each process running.

### Terminal A — `someipd` (middleware container)

```bash
cd /app/inc_someip_gateway
unset LD_LIBRARY_PATH
bazel run //src/someipd:someipd_example
```

The `LD_LIBRARY_PATH` reset was needed in the tested container environment to avoid unintended library selection.

You may observe repeated messages such as:

```text
Didn't receive a multicast SD message for 2200ms.
```

This warning alone does not establish failure. Check actual gateway reception and verifier output before diagnosing a network fault.

### Terminal B — `gatewayd` (middleware container)

```bash
cd /app/inc_someip_gateway
bazel run //src/gatewayd:gatewayd_example
```

Expected:

```text
Creating remote service instance: lidar_frame (service_id=0x1234, instance_id=0x5678, specifier=lidar/LidarFrame1)
[gatewayd] RemoteServiceInstance created for instance specifier: lidar/LidarFrame1
Gateway started, waiting for shutdown signal...
```

If you see `Could not open Shared Memory /lola-data-0000000000001337-00012`, check that `someipd` is running and that its shared-memory files exist. Restart `gatewayd` **after** `someipd` is ready; a failed proxy initialization is not necessarily retried automatically.

### Terminal C — IPC verifier (middleware container)

```bash
cd /app/inc_someip_gateway
bazel run //examples/lidar_visualizer_sim:lidar_ipc_verifier
```

Expected:

```text
[ipc-verifier] starting
[ipc-verifier] service found
[ipc-verifier] GenericProxy event found
[ipc-verifier] event storage sample size=65584 bytes
[ipc-verifier] serialized-format=false
[ipc-verifier] subscribed to lidar_frame
```

### Terminal D — LiDAR publisher (publisher container)

```bash
cd /app/publisher-node
bash scripts/lidar-node-publisher.sh
```

Start the publisher **last**, after the middleware and verifier are ready. The publisher may finish after its configured dataset is sent.

## 6. Verify the inbound pipeline

Check the gateway terminal:

```text
[gatewayd] Received SOME/IP event: event=0x8778 payload=1020B
[gatewayd] Forwarded event 0x8778 to IPC subscribers
```

Then check the verifier terminal. The successful test on 27 October 2026 produced:

```text
[ipc-verifier] ===== frame 28 =====
[ipc-verifier] slot first 32 bytes: fc 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 24 00 00 00 ...
[ipc-verifier] candidate LiDAR payload offset=16
[ipc-verifier] candidate ray_count=36
[ipc-verifier] candidate frame_timestamp=2.8001999999999998
[ipc-verifier] PASS: found possible 1020-byte LiDAR payload at offset 16
```

**Acceptance criterion for the current inbound test:** The gateway receives 1020-byte SOME/IP events and the verifier locates the expected 36-ray payload header at offset 16.

This `PASS` is a **structural/header check**, not a complete byte-for-byte comparison of all 1020 bytes and not a proof that every ray field is correct.

### Validation matrix

| Path | Status | Test |
| --- | --- | --- |
| External SOME/IP → `someipd` → `gatewayd` → LoLa → verifier | **Verified** | 1020-byte payload recognized, ray count 36 |
| LoLa subscriber → LoLa publisher → outbound `gatewayd`/`someipd` | Not yet verified | Implement and capture outbound event |
| External publisher → external receiver | Not yet verified | Match frames and compare all 1020 application bytes |

For a future full integrity test, capture the 1020-byte **application payload** at the source and destination, pair corresponding frames by timestamp or sequence identifier, and compare each byte. Do not include SOME/IP headers or LoLa sample prefixes in the application-payload comparison.

## 7. Troubleshooting

### Gateway receives events, verifier cannot find `ray_count=36`

Typical signs:

```text
[gatewayd] Received SOME/IP event: event=0x8778 payload=1020B
[ipc-verifier] FAIL: ray_count=36 not found in sample storage
```

Likely cause in the tested fresh-clone environment: the three LoLa corrections from Section 4 are missing. Verify all three source edits, rebuild `gatewayd`, stop the old processes, and restart the pipeline. A running process may continue using an old executable even after a successful rebuild.

### Gateway cannot open LoLa instance `00012`

```text
Could not open Shared Memory /lola-data-0000000000001337-00012
Proxy creation failed for 'lidar/LidarFrame1'
```

Start `someipd` first and check:

```bash
ls -lah /dev/shm/lola-*
```

In the tested setup, `someipd` created:

```text
lola-ctl-0000000000001337-00012
lola-data-0000000000001337-00012
```

Restart `gatewayd` after these files exist.

### Shared-memory startup errors after a failed run

Stale LoLa objects may remain in `/dev/shm`. **Never remove shared-memory objects while active processes are using them.** First stop the publisher and middleware processes, and verify no relevant process remains:

```bash
ps -ef | grep -E 'gatewayd_example|someipd_example|lidar_ipc_verifier' | grep -v grep
```

A process shown as `<defunct>` is a zombie and no longer maps shared memory. If processes resist graceful termination, investigate before force-killing them.

To inspect active LoLa mappings without `fuser`:

```bash
grep -l '/dev/shm/lola-' /proc/[0-9]*/maps 2>/dev/null
```

No output means no LoLa mappings are visible to the current container's process namespace. This is **not a guarantee** that another container or host process is not using the same shared-memory mount; check other participants before deleting anything.

Only after confirming the shared-memory mount is not in use, list its contents:

```bash
ls -lah /dev/shm/lola-*
```

In the tested environment, the following six stale files were removed after the relevant processes had exited:

```text
lola-ctl-0000000000001337-00000
lola-ctl-0000000000001337-00012
lola-ctl-0000000000004660-22136
lola-data-0000000000001337-00000
lola-data-0000000000001337-00012
lola-data-0000000000004660-22136
```

**Do not blindly delete these paths on a shared machine.** Identify which objects belong to your stopped test run, and delete only those objects after checking all containers that share `/dev/shm`. Recreate them by restarting services in the documented order.

### `someipd` multicast Service Discovery warnings

Warnings about missing multicast SD messages are not by themselves evidence of failed payload delivery. Confirm the Docker network, publisher address, configured service/event IDs, and whether `gatewayd` actually receives events.

### Logging configuration warnings

A message such as `No logging configuration files could be found. Fallback to console logging.` was observed during a successful test and is not by itself fatal.

### Bazel dependency source missing or reset

The workaround targets a Bazel-fetched dependency. The directory may not exist until Bazel resolves the external repository, and local modifications may be lost after cache cleaning, dependency refresh, or a new checkout. Reapply and verify the edits before rebuilding, or preferably replace this manual process with a committed Bazel patch.

## 8. Development roadmap

The following work remains to complete the intended bidirectional pipeline:

1. Extend the existing LiDAR subscriber so it republishes the received serialized application payload through a LoLa skeleton event. A separate microservice is not required solely to test IPC forwarding.
2. Configure the outbound `gatewayd` binding to consume the new LoLa event and send a corresponding SOME/IP event through `someipd`.
3. Implement or configure an external SOME/IP receiver to subscribe and capture frames.
4. Add a frame-by-frame, 1020-byte payload comparator and report mismatches, dropped frames, and ordering.
5. Convert the three manual `score_communication` edits into a reproducible, version-pinned dependency patch committed to the repository.

Consult `docs/LIDAR_COMMUNICATION_ROADMAP.md` **if that file exists in your checkout** for further implementation notes. This README is the main entry point; a separate document should not be required to run the verified inbound test.

## 9. Known limitations

- The Docker image build and initial host setup steps have not been established from the supplied README; they must be documented using the repository's actual Docker configuration.
- The current LoLa workaround is manually applied to Bazel external source, so GitHub alone does not yet reproduce the fixed dependency.
- The `PASS` output checks the expected payload structure, not complete byte equality.
- The outbound IPC → SOME/IP path and external receiver remain future work.
- The observed sample layout, offsets, and startup dependencies are specific to the tested implementation and may change with dependency revisions.

## 10. Contributor checklist

Before reporting an inbound communication failure, confirm that both Docker containers are running, all three LoLa corrections are applied, `gatewayd` builds successfully, `someipd` starts before `gatewayd`, the verifier subscribes, and the publisher transmits 1020-byte frames. Record the Bazel dependency revision, relevant logs, and whether the verifier finds `ray_count=36` at offset 16.

---

**Tested baseline:** 27 October 2026; Eclipse S-CORE `mw::com` / LoLa; vSomeIP 3.6.1 observed in logs; simulated LiDAR SOME/IP event `0x8778`, 1020-byte payload, 36 rays. Other dependency versions, host platforms, and container configurations require independent validation.
