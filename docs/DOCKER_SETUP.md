
# Setup and operation

[Project overview](../README.md) · [LoLa workaround](LOLA_DEPENDENCY_FIX.md) · [Test criteria](ARCHITECTURE_AND_TESTING.md)

This procedure follows the **repository-root** `docker-compose.yml` (not `middleware/docker-compose.yml`). The root Compose file builds two Ubuntu 24.04 images, uses `publisher-node` and `middleware-node`, and binds the source folders into the containers. Its named `sdv-net` network is external and therefore must exist before `docker compose up`.

## 1. Host prerequisites

- Linux x86_64 host with Docker Engine and the Compose plugin, Git, enough disk space for Bazel/toolchains, and Internet access for first-time Docker/Bazel downloads.
- Permissions to execute `docker` (or run through suitable administrative privileges).
- Available Docker subnet `172.31.0.0/24`; change Compose and both vSomeIP JSON configurations together if it conflicts with a host VPN or existing route.
- Dockerfiles install build tools, CMake, Bazelisk 1.20.0 launcher and build vSomeIP from upstream. They do **not** pin the checkout of COVESA/vSomeIP to a commit, so complete reproducibility of fresh image builds is not established.

## 2. Clone and prepare Docker network

```bash
git clone https://github.com/TinaMoafi/publisher-score.git
cd publisher-score
# Only when this network does not already exist:
docker network inspect sdv-net >/dev/null 2>&1 || docker network create --subnet 172.31.0.0/24 sdv-net
# Inspect effective configuration and identify unexpected overrides:
docker compose config
docker compose up -d --build
docker compose ps
```

Expected containers: `publisher-node` at `172.31.0.10` and `middleware-node` at `172.31.0.20`. Their `sleep infinity` command keeps containers available for interactive commands; it does not automatically run either application. If a `sdv-net` network already exists, verify its subnet matches the fixed Compose addresses.

```bash
docker compose exec publisher-node ip -4 addr show eth0
docker compose exec middleware-node ip -4 addr show eth0
docker compose exec publisher-node ping -c 2 172.31.0.20
```

## 3. Fetch dependency, apply LoLa workaround, rebuild

`score_communication` is declared in `middleware/inc_someip_gateway/MODULE.bazel` as version `0.2.1`. Run an initial Bazel build to fetch it; on a fresh clone the first build may produce the uncorrected binary. **Do not run the transport test until the patch and rebuild finish.**

```bash
docker compose exec middleware-node bash -lc 'cd /app/inc_someip_gateway && bazel build //src/gatewayd:gatewayd_example'
```

The helper script resides in the **host repository** at `scripts/apply_lola_patch.sh`; the root Compose file does not mount the host repository root. The following copies both the patch and helper into the already-mounted middleware project folder, preserving them as host-visible files:

```bash
mkdir -p middleware/inc_someip_gateway/local_lola_patch
cp patches/score_communication_lola_generic_buffer.patch middleware/inc_someip_gateway/local_lola_patch/
cp scripts/apply_lola_patch.sh middleware/inc_someip_gateway/local_lola_patch/
docker compose exec middleware-node bash -lc 'cd /app/inc_someip_gateway && bash local_lola_patch/apply_lola_patch.sh'
```

The helper uses `git apply --check` and refuses unrelated dependency layouts. If it reports a mismatch, **do not force the patch**; review [the dependency fix](LOLA_DEPENDENCY_FIX.md). This bundled patch is reconstructed from the verified three edits, but applying it to a pristine checkout of the declared dependency revision has **not** been independently verified. Do not claim automatic patch integration; the Bazel external source is mutable and may be reset on cache refresh.

```bash
docker compose exec middleware-node bash -lc 'cd /app/inc_someip_gateway && bazel build //src/gatewayd:gatewayd_example'
```

## 4. Start services in separate terminals

Use one host terminal for each command and leave it running. The order is important: `someipd` creates the LoLa instance which `gatewayd` opens.

**Terminal 1 — SOME/IP daemon:**

```bash
cd publisher-score
docker compose exec middleware-node bash -lc 'cd /app/inc_someip_gateway && unset LD_LIBRARY_PATH && bazel run //src/someipd:someipd_example'
```

**Terminal 2 — gateway:**

```bash
cd publisher-score
docker compose exec middleware-node bash -lc 'cd /app/inc_someip_gateway && bazel run //src/gatewayd:gatewayd_example'
```

Expected message: `[gatewayd] RemoteServiceInstance created for instance specifier: lidar/LidarFrame1`.

**Terminal 3 — LoLa IPC verifier:**

```bash
cd publisher-score
docker compose exec middleware-node bash -lc 'cd /app/inc_someip_gateway && bazel run //examples/lidar_visualizer_sim:lidar_ipc_verifier'
```

Expected messages: `service found`, `GenericProxy event found`, `subscribed to lidar_frame`.

**Terminal 4 — publisher (start last):**

```bash
cd publisher-score
docker compose exec publisher-node bash -lc 'cd /app/publisher-node && bash scripts/lidar-node-publisher.sh'
```

The publisher script sets `VSOMEIP_CONFIGURATION`, adds a multicast route when needed, builds the publisher with CMake, then runs `build/lidar_csv_publisher`. Its input file is `data/lidar_data.csv` relative to `/app/publisher-node`.

## 5. Verify success

Gateway: `Received SOME/IP event: event=0x8778 payload=1020B` and `Forwarded event 0x8778 to IPC subscribers`.

Verifier: `candidate LiDAR payload offset=16`, `candidate ray_count=36`, and `PASS: found possible 1020-byte LiDAR payload at offset 16`.

This only establishes recognition of the expected frame format; see [test protocol](ARCHITECTURE_AND_TESTING.md) for acceptance criteria and limitations.

## 6. Stop and reset

Stop the publisher and middleware commands with Ctrl+C in their own terminals. Run `docker compose ps`, and check for surviving service processes before attempting restarts. To stop/remove the two containers, run `docker compose down` from the repository root; the external `sdv-net` network remains.

Do **not** indiscriminately delete `/dev/shm/lola-*`. Verify no process in any container sharing that mount has a mapped LoLa object before deleting precisely identified stale files. [Troubleshooting](TROUBLESHOOTING.md) includes checks and the limitations of `/proc` inspection.

## 7. Known setup boundaries

The nested `middleware/docker-compose.yml` is not equivalent to the root Compose file; it has a different external network name and does not mount the middleware source folder. Ubuntu 24.04 is the container base even when the **host** is Debian. The project's Bazel toolchain configuration may download additional dependencies; first builds can be slow or fail without external network access.
