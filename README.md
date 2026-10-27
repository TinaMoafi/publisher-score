
# LiDAR SOME/IP–LoLa Gateway | Eclipse S-CORE

A two-container integration project demonstrating delivery of simulated LiDAR frames from a C++ vSomeIP publisher into Eclipse S-CORE `mw::com`/LoLa shared-memory IPC.

**Verified baseline (27 October 2026):** inbound SOME/IP → `someipd` → `gatewayd` → LoLa → `lidar_ipc_verifier`. A 1020-byte application payload containing 36 rays is recognized at LoLa sample offset 16. This is a structural reception test, **not** a byte-for-byte integrity or outbound-forwarding test.

**Status:** inbound path verified; IPC→SOME/IP forwarding and external receiver planned. The `score_communication` 0.2.1 dependency requires three local LoLa corrections; see [Dependency fix](docs/LOLA_DEPENDENCY_FIX.md). Do **not** assume a clean clone works without applying the workaround. The patch is included but has not been clean-clone tested.

## Architecture

```text
publisher-node (172.31.0.10)                middleware-node (172.31.0.20)
CSV -> LiDAR C++ publisher -- SOME/IP --> someipd -> gatewayd -- LoLa --> IPC verifier
                                     service 0x1234 / instance 0x5678 / event 0x8778

Planned: IPC subscriber/forwarder -> LoLa -> gatewayd -> someipd -> external SOME/IP receiver
```

## Quick start

Requirements: Linux host with Docker Engine, Docker Compose plugin, Git, network access for image builds and Bazel downloads. The Dockerfiles use Ubuntu 24.04 and build COVESA vSomeIP from its repository. The root Compose file refers to an **external** Docker network called `sdv-net`: create it explicitly before starting the containers.

```bash
# Host shell
git clone https://github.com/TinaMoafi/publisher-score.git
cd publisher-score
docker network inspect sdv-net >/dev/null 2>&1 || docker network create --subnet 172.31.0.0/24 sdv-net
docker compose up -d --build
docker compose ps
```

Next follow [Setup and operation](docs/DOCKER_SETUP.md) to fetch the Bazel dependency, copy/apply the bundled workaround into the mounted middleware tree, rebuild, and launch each service in its own terminal. Do not run the uncorrected gateway build as the final test.

For the authoritative, complete sequence—including patch application, build, service start order, verification and cleanup—follow [Setup and operation](docs/DOCKER_SETUP.md). The normal order is **someipd → gatewayd → verifier → publisher**. Starting gatewayd first caused missing LoLa instance `00012` in the tested environment.

## Project navigation

| Document | What it covers |
| --- | --- |
| [Setup and operation](docs/DOCKER_SETUP.md) | Clone, Docker network, Compose, build, startup and shutdown |
| [Architecture and test protocol](docs/ARCHITECTURE_AND_TESTING.md) | Components, interfaces, serialized layout, tests and scope |
| [LoLa dependency workaround](docs/LOLA_DEPENDENCY_FIX.md) | Root cause, version-specific three-line fix, reproducibility risks |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | Symptoms, diagnosis, shared-memory safety |
| [Development roadmap](docs/DEVELOPMENT.md) | Extending the subscriber to outgoing IPC and SOME/IP |

## Repository map

```text
publisher-score/
  docker-compose.yml                   # authoritative two-node Compose setup
  publisher/                           # simulated SOME/IP publisher, CSV data, vsomeip configuration
  middleware/
    docker/middleware-node.Dockerfile  # image definition
    configs/vsomeip/                   # middleware network configuration
    inc_someip_gateway/                # S-CORE Bazel module, gateway and verifier
  docs/                                # project-specific documentation
  patches/                             # version-specific dependency patch
  scripts/                             # patch helper
```

Do not use `middleware/docker-compose.yml` interchangeably with the repository-root Compose file: it is a separate, middleware-only configuration with a different external network name.

## Interface contract

SOME/IP service `0x1234`, instance `0x5678`, event/eventgroup `0x8778` on the tested setup. Publisher serializes `ray_count` (uint32), `frame_timestamp` (double), then 36 ray records (each timestamp double, ray ID uint32, x/y/z/intensity floats): `4 + 8 + 36 × 28 = 1020` bytes. Serialization uses native byte copies on the test system; see [Architecture](docs/ARCHITECTURE_AND_TESTING.md) for portability limitations.

## Evidence and limitations

The successful verifier emits `candidate LiDAR payload offset=16`, `candidate ray_count=36` and `PASS: found possible 1020-byte LiDAR payload at offset 16`. Tests must still establish full payload equality, frame loss and the reverse path. vSomeIP multicast warnings alone do not imply transport failure.

Project-specific files are maintained here; Eclipse S-CORE subproject documentation and licensing remain in `middleware/inc_someip_gateway/`.
