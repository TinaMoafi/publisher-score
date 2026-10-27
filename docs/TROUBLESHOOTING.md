
# Troubleshooting and safe recovery

[Setup](DOCKER_SETUP.md) · [LoLa fix](LOLA_DEPENDENCY_FIX.md) · [Test criteria](ARCHITECTURE_AND_TESTING.md)

## Gateway creates proxy before someipd

Symptom: `Could not open Shared Memory /lola-data-0000000000001337-00012` and `Proxy creation failed for 'lidar/LidarFrame1'`. In the tested setup, `someipd` creates the needed instance. Start `someipd` **first**, verify it initializes, then start/restart `gatewayd`. Once proxy creation has failed, continuing to run the gateway may not retry that operation.

## Gateway receives 1020 bytes; verifier fails

Symptom: `Received SOME/IP event: event=0x8778 payload=1020B` alongside `FAIL: ray_count=36 not found in sample storage`. The fresh-clone investigation traced this to unpatched LoLa GenericEvent buffer handling. Check [the three corrections](LOLA_DEPENDENCY_FIX.md), rebuild, and **restart** gateway processes so they load the newly built executable.

## No events reach the gateway

Check `docker compose ps`, container IPs, `VSOMEIP_CONFIGURATION`, service `0x1234`, instance `0x5678`, event/eventgroup `0x8778`, publisher port `30509`, SD multicast `224.224.224.245:30490`, and the publisher startup logs. On the host, `docker compose exec publisher-node ping -c 2 172.31.0.20` checks basic connectivity but **not** multicast/SOME/IP correctness.

## Service Discovery warnings

`Didn't receive a multicast SD message for 2200ms` may appear even while payloads are delivered. Do not treat it as proof of failure without checking gateway events and verifier results.

## Shared-memory recovery

1. Stop publisher, verifier, gateway and `someipd` cleanly in their terminals.
2. Check `ps -ef | grep -E 'gatewayd_example|someipd_example|lidar_ipc_verifier' | grep -v grep` in middleware container; stop surviving live processes. A `<defunct>` process is already exited.
3. Run `grep -l '/dev/shm/lola-' /proc/[0-9]*/maps 2>/dev/null` to inspect current-container mappings. **No output is only evidence for the current PID namespace**, not other containers or the host.
4. Inspect `ls -lah /dev/shm/lola-*`; confirm no other process/container sharing that `/dev/shm` mount uses those objects. Only then remove the **specific** stale objects from that test session; do not run a blanket `rm -f /dev/shm/*`.
5. Restart in the documented order.

## Patch cannot apply

Do not change line numbers at random or force apply. Confirm `MODULE.bazel` uses `score_communication` 0.2.1, locate the Bazel external checkout, inspect both C++ functions, and compare with [the fix description](LOLA_DEPENDENCY_FIX.md). The patch is version-sensitive. If dependency source already contains equivalent corrections, the helper may report `already applied`.

## Logging config fallback

`No logging configuration files could be found. Fallback to console logging.` was observed during a successful run and is nonfatal if the services continue.
