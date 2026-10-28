# Troubleshooting and Safe Recovery

[Setup](DOCKER_SETUP.md) · [LoLa fix](LOLA_DEPENDENCY_FIX.md) · [Architecture and testing](ARCHITECTURE_AND_TESTING.md)

This document describes common build, configuration, networking, service-discovery, and LoLa IPC problems encountered in the LiDAR SOME/IP–LoLa Gateway integration.

All commands are executed from the repository root on the Linux host unless otherwise specified.

## 1. someipd Exits During Startup

### Symptoms

The Bazel build completes successfully, but `someipd` exits immediately:

```text
Build completed successfully
Running command line: bazel-bin/src/someipd/someipd_example.exe
mw::log initialization error:
No logging configuration files could be found.
Fallback to console logging.
SOMEIPD_EXIT_CODE=255
```

### Possible cause

During initialization, `someipd` searches for the internal IPC service:

```text
someipd/gatewayd_messages
```

The implementation uses `SomeipMessageTransferProxy::FindService()` to discover this service before entering its routing loop.

If the required service is unavailable, initialization may fail.

### Recovery

Start the services in the following order:

1. `gatewayd`
2. `someipd`
3. `lidar_ipc_verifier`
4. LiDAR publisher

Start `gatewayd`:

```bash
docker compose exec middleware-node bash -lc \
'cd /app/inc_someip_gateway && bazel run //src/gatewayd:gatewayd_example'
```

In another terminal, start `someipd`:

```bash
docker compose exec middleware-node bash -lc \
'cd /app/inc_someip_gateway && unset LD_LIBRARY_PATH && bazel run //src/someipd:someipd_example'
```

Expected:

```text
[someipd] Starting routing loop...
[someipd] Starting network stack processing...
```

These messages confirm that initialization progressed into network processing.

If `someipd` still exits, check its configuration and the status of the gateway service. Exit code `255` alone does not establish the exact cause.

## 2. Gateway Cannot Find the Required LoLa Instance

### Symptoms

```text
Could not open Shared Memory /lola-data-0000000000001337-00012
Proxy creation failed for 'lidar/LidarFrame1'
```

### Possible cause

The required LoLa instance was not available when the gateway attempted to create its proxy.

Some service instances are created during application initialization, and unsuccessful proxy creation may not be retried automatically.

### Recovery

Check the gateway and SOME/IP daemon processes:

```bash
docker compose exec middleware-node bash -lc \
'ps -ef | grep -E "[g]atewayd_example|[s]omeipd_example" || true'
```

Verify that the required service instances are available.

Restart the services using the startup procedure in [Setup and operation](DOCKER_SETUP.md).

If gateway initialization fails because an IPC instance is unavailable, inspect the corresponding service manifest and startup logs. Do not repeatedly restart applications without identifying which endpoint is missing.

## 3. SOME/IP Configuration Binary Is Missing

### Symptoms

The application exits during initialization, or the expected configuration file cannot be found:

```text
src/config/mw_someip_config.bin:
No such file or directory
```

### Cause

The SOME/IP gateway uses a generated FlatBuffers binary configuration.

The source configuration is located at:

```text
src/config/mw_someip_config.json
```

The generated configuration is expected at:

```text
src/config/mw_someip_config.bin
```

### Recovery

Check the Bazel-generated file:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway
ls -lh bazel-bin/src/config/mw_someip_config.bin
'
```

If it exists, copy it to the runtime location:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway

cp -L bazel-bin/src/config/mw_someip_config.bin \
  src/config/mw_someip_config.bin
'
```

Verify both files:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway

sha256sum \
  src/config/mw_someip_config.bin \
  bazel-bin/src/config/mw_someip_config.bin
'
```

The checksums should match.

If the Bazel-generated file does not exist, investigate its configuration-generation rule. Do not copy binaries from unrelated builds or repository revisions.

## 4. Gateway Receives LiDAR Events but IPC Verifier Fails

### Symptoms

The gateway reports:

```text
Received SOME/IP event: event=0x8778 payload=1020B
```

But the IPC verifier reports:

```text
FAIL: ray_count=36 not found in sample storage
```

### Cause

The `score_communication` version `0.2.1` generic-event implementation requires corrections to its shared-memory sample-buffer handling in the tested environment.

These corrections are described in [LoLa dependency workaround](LOLA_DEPENDENCY_FIX.md).

### Recovery

Verify the active Bazel dependency:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway

G="$(bazel info output_base)/external/score_communication+/score/mw/com/impl/bindings/lola"

grep -nF "return event_data_raw_array;" "$G/skeleton_memory_manager.cpp"

grep -nF "OpenEventDataFromOpenedSharedMemory<std::max_align_t>" "$G/skeleton.cpp"

grep -nF "static_cast<void*>(event_data_storage.data())" "$G/skeleton.cpp"
'
```

If the corrections are absent, apply the bundled manual patch following the setup guide.

Rebuild `gatewayd` after applying the patch:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway
bazel build //src/gatewayd:gatewayd_example
'
```

Stop the previous gateway process before starting the rebuilt executable.

The expected verifier result is:

```text
candidate LiDAR payload offset=16
candidate ray_count=36
PASS: found possible 1020-byte LiDAR payload at offset 16
```

## 5. Publisher Sends Frames but Gateway Receives Nothing

### Symptoms

The LiDAR publisher reports transmitted frames, but `gatewayd` does not report incoming SOME/IP events.

### Network checks

Verify both containers:

```bash
docker compose ps
```

Verify IP addresses:

```bash
docker compose exec publisher-node ip -4 addr show eth0
docker compose exec middleware-node ip -4 addr show eth0
```

Expected:

```text
publisher-node: 172.31.0.10
middleware-node: 172.31.0.20
```

Test basic connectivity:

```bash
docker compose exec publisher-node ping -c 3 172.31.0.20
```

Successful ping responses confirm IP connectivity, but not SOME/IP communication.

### Verify multicast routing

```bash
docker compose exec publisher-node ip route
docker compose exec middleware-node ip route
```

The middleware should have:

```text
224.0.0.0/4 dev eth0 scope link
```

If missing, add it:

```bash
docker compose exec -u root middleware-node \
  ip route replace 224.0.0.0/4 dev eth0
```

### Verify vSomeIP configuration

Publisher:

```bash
docker compose exec publisher-node bash -lc \
'cat /app/publisher-node/config/vsomeip/publisher-container.json'
```

Middleware:

```bash
docker compose exec middleware-node bash -lc \
'cat /app/config/vsomeip/middleware-container.json'
```

Check:

| Parameter | Expected value |
|---|---|
| Service | `0x1234` |
| Instance | `0x5678` |
| Event | `0x8778` |
| Eventgroup | `0x8778` |
| Service Discovery multicast | `224.224.224.245` |
| Service Discovery port | UDP `30490` |
| LiDAR event port | UDP `30509` |

All identifiers and network endpoints must be consistent across the relevant applications and configuration files.

## 6. SOME/IP Service Discovery Is Visible but No LiDAR Events Arrive

### Symptoms

Network capture shows packets such as:

```text
172.31.0.10.30490 > 224.224.224.245.30490
```

But no LiDAR event traffic is observed.

### Explanation

These packets demonstrate that SOME/IP Service Discovery messages are transmitted and visible at the middleware network interface.

They do not prove that `someipd` discovered the publisher, subscribed to the eventgroup, or received application data.

### Diagnosis

Capture traffic inside `middleware-node`:

```bash
docker compose exec middleware-node bash -lc \
'tcpdump -ni eth0 "udp port 30490 or udp port 30509"'
```

Check UDP sockets:

```bash
docker compose exec middleware-node bash -lc \
'ss -lunp | grep -E "30490|30509" || true'
```

Inspect `someipd` and publisher logs for service discovery and subscription activity.

If LiDAR event packets reach the middleware interface but are not processed, investigate the `someipd` runtime configuration and the gateway IPC connection.

## 7. SOME/IP Multicast Warnings

The vSomeIP runtime may report warnings such as:

```text
Didn't receive a multicast SD message for 2200ms
```

These warnings may appear even when application payloads are delivered.

Do not treat them alone as proof of a communication failure.

Confirm actual event reception using the gateway logs and IPC verifier output.

## 8. LoLa Service-Instance Lock Failure

### Symptoms

```text
Flock try_lock failed:
Another Skeleton could have already flocked the markerfile
and is actively offering the same service instance.
```

Additional errors may include:

```text
Skeleton service element could not be created
```

### Cause

Another process may still own the LoLa service-instance lock.

This can occur when old middleware processes remain active after terminals are closed.

### Recovery

List running processes:

```bash
docker compose exec middleware-node bash -lc \
'ps -ef | grep -E "[s]omeipd_example|[g]atewayd_example|[l]idar_ipc_verifier" || true'
```

Stop any duplicate processes belonging to the test.

Use graceful termination where possible before considering stronger process termination.

Check again to ensure that the relevant processes have exited.

Do not remove shared-memory files while they are still in use.

## 9. Safe Shared-Memory Recovery

LoLa uses shared-memory objects and service-discovery resources.

Incorrect cleanup can interfere with active middleware processes.

Recommended procedure:

1. Stop the publisher, IPC verifier, `someipd` and `gatewayd` through their respective terminals.
2. Check for surviving processes inside `middleware-node`.
3. Inspect shared-memory objects and their current users.
4. Confirm that no other process or container is using an identified object before deleting it.
5. Remove only stale objects specifically associated with the stopped test session, if necessary.
6. Restart the services following [Setup and operation](DOCKER_SETUP.md).

Inspect shared memory:

```bash
docker compose exec middleware-node bash -lc '
ls -lah /dev/shm/
'
```

Check mappings visible in the container's PID namespace:

```bash
docker compose exec middleware-node bash -lc '
grep -l "/dev/shm/lola-" /proc/[0-9]*/maps 2>/dev/null || true
'
```

An empty result does not prove that the objects are unused by processes outside the container's PID namespace.

Never indiscriminately execute:

```bash
rm -f /dev/shm/*
```

## 10. LoLa Patch Cannot Be Applied

### Symptoms

```text
ERROR: patch does not match dependency contents
```

### Diagnosis

Verify the dependency version:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway
grep -n "score_communication" MODULE.bazel
'
```

Find the actual Bazel external dependency:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway

bazel info output_base

find "$(bazel info output_base)/external" \
  -name skeleton_memory_manager.cpp 2>/dev/null
'
```

The patch targets `score_communication` version `0.2.1`.

Do not modify line numbers randomly or force the patch against an unknown dependency version.

If the dependency already contains equivalent corrections, inspect the source and confirm whether the patch is already applied.

## 11. Bazel Builds Successfully but Runtime Fails

A successful Bazel build confirms compilation and linking, not correct runtime initialization.

When an application exits unexpectedly, verify:

- Required configuration files.
- Service-instance manifests.
- Process startup order.
- LoLa service availability.
- Duplicate service instances.
- Dynamic-library dependencies.
- Network and multicast configuration.

Check unresolved shared libraries:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway

ldd bazel-bin/src/someipd/someipd_example.exe | grep "not found" || true
'
```

For unresolved startup failures, system-call tracing with `strace` can help identify missing files, permission problems or failed resource operations.

A missing file during a normal library-search sequence is not necessarily fatal. Interpret system-call errors together with the application's initialization code and exit status.

## 12. Logging Configuration Fallback

A warning such as:

```text
No logging configuration files could be found.
Fallback to console logging.
```

means that the S-CORE logging framework could not locate its logging configuration.

The runtime falls back to console logging.

This message alone is not fatal. It was also observed during successful middleware execution.

If the application exits after this warning, investigate its initialization and exit code instead of assuming logging is responsible.

## 13. Docker Network Conflicts

### Symptoms

```text
Pool overlaps with other one on this address space
```

This indicates a Docker subnet conflict.

List networks:

```bash
docker network ls
```

Inspect subnet assignments:

```bash
docker network ls -q | xargs -r docker network inspect \
  --format '{{.Name}}: {{range .IPAM.Config}}{{.Subnet}} {{end}}'
```

The project uses:

```text
Network: sdv-net
Subnet: 172.31.0.0/24
Publisher: 172.31.0.10
Middleware: 172.31.0.20
```

Do not remove unrelated Docker networks or containers without confirming that they are no longer required.

## 14. Final Verification

After resolving a failure, repeat the complete end-to-end procedure from [Setup and operation](DOCKER_SETUP.md).

Successful structural reception requires:

```text
Gateway:
Received SOME/IP event: event=0x8778 payload=1020B
Forwarded event 0x8778 to IPC subscribers
```

And:

```text
IPC Verifier:
candidate LiDAR payload offset=16
candidate ray_count=36
PASS: found possible 1020-byte LiDAR payload at offset 16
```

These results confirm structural recognition of the expected LiDAR frame through the inbound SOME/IP–LoLa communication path.

They do not establish byte-for-byte integrity, zero frame loss, timing performance or outbound IPC → SOME/IP forwarding.

## References

- [Setup and operation](DOCKER_SETUP.md)
- [Architecture and testing](ARCHITECTURE_AND_TESTING.md)
- [LoLa dependency workaround](LOLA_DEPENDENCY_FIX.md)
- [Development roadmap](DEVELOPMENT.md)
