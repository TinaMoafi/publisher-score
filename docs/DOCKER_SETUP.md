# LiDAR SOME/IP–LoLa Gateway — Setup and Testing Guide

This document describes the complete procedure for building, configuring, running, and verifying the LiDAR SOME/IP–LoLa Gateway integration using Eclipse S-CORE.

The test demonstrates communication from an external LiDAR publisher to the S-CORE middleware through SOME/IP, followed by forwarding into LoLa shared-memory IPC.

The setup uses two Docker containers and the existing repository structure. The LoLa dependency correction and middleware multicast route are configured manually.

## 1. System Architecture

```text
publisher-node (172.31.0.10)
+--------------------------------+
|                                |
| LiDAR CSV Dataset              |
|         |                      |
|         v                      |
| C++ LiDAR Publisher            |
|         |                      |
|     vSomeIP                    |
|         |                      |
+---------|----------------------+
          |
          | SOME/IP over Ethernet
          |
          v
middleware-node (172.31.0.20)
+--------------------------------+
|                                |
|       someipd                  |
|          |                     |
|          | Gateway IPC         |
|          v                     |
|       gatewayd                 |
|          |                     |
|          | mw::com / LoLa      |
|          v                     |
|    lidar_ipc_verifier          |
|          |                     |
|          v                     |
|     Payload Validation         |
|                                |
+--------------------------------+
```

The application uses the following SOME/IP identifiers:

| Parameter | Value |
|---|---|
| Service ID | `0x1234` |
| Instance ID | `0x5678` |
| Event ID | `0x8778` |
| Eventgroup ID | `0x8778` |
| SOME/IP-SD multicast | `224.224.224.245` |
| SOME/IP-SD port | UDP `30490` |
| LiDAR event port | UDP `30509` |
| Expected payload | 1020 bytes |
| LiDAR rays per frame | 36 |

## 2. Prerequisites

The host Linux system requires:

- Docker Engine
- Docker Compose plugin
- Git
- Internet access for downloading dependencies
- Sufficient memory and storage for Bazel builds
- An available `172.31.0.0/24` Docker subnet

Verify:

```bash
git --version
docker --version
docker compose version
docker info
```

The project containers use Ubuntu 24.04. Bazel and the required C++ development tools are installed through the Dockerfiles.

All following commands are executed from the Linux host unless explicitly stated otherwise.

## 3. Clone the GitHub Repository

Create a working directory:

```bash
mkdir -p ~/work
cd ~/work
```

Clone the repository:

```bash
git clone https://github.com/TinaMoafi/publisher-score.git
```

Enter the project:

```bash
cd publisher-score
```

Verify the repository:

```bash
git status
git branch --show-current
ls
```

Expected project structure:

```text
publisher-score/
├── docker-compose.yml
├── README.md
├── publisher/
├── middleware/
│   ├── docker/
│   ├── configs/
│   └── inc_someip_gateway/
├── patches/
├── scripts/
└── docs/
```

Use the `docker-compose.yml` located in the repository root.

Do not substitute the separate `middleware/docker-compose.yml`, which uses a different configuration.

## 4. Configure the Docker Network

Create the required external Docker network:

```bash
docker network inspect sdv-net >/dev/null 2>&1 || \
docker network create --subnet 172.31.0.0/24 sdv-net
```

Verify:

```bash
docker network inspect sdv-net
```

Expected subnet:

```text
172.31.0.0/24
```

If Docker reports that the subnet overlaps with another network, inspect existing networks:

```bash
docker network ls
```

```bash
docker network ls -q | xargs -r docker network inspect \
  --format '{{.Name}}: {{range .IPAM.Config}}{{.Subnet}} {{end}}'
```

Do not delete unrelated networks. Resolve any subnet conflicts before proceeding. If an existing `sdv-net` network is reused, verify its subnet is `172.31.0.0/24`.

## 5. Build and Start Docker Containers

From the repository root:

```bash
cd ~/work/publisher-score
```

Validate Docker Compose:

```bash
docker compose config -q
```

Build and start both containers:

```bash
docker compose up -d --build
```

Verify:

```bash
docker compose ps
```

Expected running containers:

```text
publisher-node
middleware-node
```

The containers use `sleep infinity` to remain available for interactive execution. Starting the containers does not automatically start the SOME/IP gateway services.

## 6. Verify Container Networking

Check the publisher IP:

```bash
docker compose exec publisher-node ip -4 addr show eth0
```

Expected:

```text
172.31.0.10/24
```

Check the middleware IP:

```bash
docker compose exec middleware-node ip -4 addr show eth0
```

Expected:

```text
172.31.0.20/24
```

Test connectivity:

```bash
docker compose exec publisher-node ping -c 3 172.31.0.20
```

Successful replies confirm IP connectivity between the containers.

## 7. Configure Multicast Routing

SOME/IP Service Discovery uses multicast communication.

The publisher startup script configures the publisher-side multicast route. The middleware route must be configured manually.

Run:

```bash
docker compose exec -u root middleware-node \
  ip route replace 224.0.0.0/4 dev eth0
```

Verify:

```bash
docker compose exec middleware-node ip route
```

Expected middleware routes include:

```text
default via 172.31.0.1 dev eth0
172.31.0.0/24 dev eth0
224.0.0.0/4 dev eth0 scope link
```

The multicast route is not persistent across container recreation. Repeat this step whenever `middleware-node` is recreated.

## 8. Fetch Bazel Dependencies

Build the gateway once to fetch the required dependencies:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway
bazel build //src/gatewayd:gatewayd_example
'
```

The gateway uses the `score_communication` dependency version `0.2.1`.

This initial build fetches the dependency but does not guarantee that the required LoLa corrections are applied.

Do not begin the communication test until the dependency is patched and rebuilt.

## 9. Apply the Manual LoLa Patch

The LoLa generic-event implementation requires three corrections related to the shared-memory sample buffer.

The repository includes:

```text
patches/score_communication_lola_generic_buffer.patch
scripts/apply_lola_patch.sh
```

### 9.1 Prepare the patch

From the host repository root:

```bash
cd ~/work/publisher-score

mkdir -p middleware/inc_someip_gateway/local_lola_patch
```

Copy the patch:

```bash
cp patches/score_communication_lola_generic_buffer.patch \
  middleware/inc_someip_gateway/local_lola_patch/
```

Copy the patch helper:

```bash
cp scripts/apply_lola_patch.sh \
  middleware/inc_someip_gateway/local_lola_patch/
```

### 9.2 Apply the patch

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway
bash local_lola_patch/apply_lola_patch.sh
'
```

Expected output:

```text
Applied LoLa patch. Rebuild gatewayd and run the verifier.
```

If it was already applied:

```text
LoLa patch already applied.
```

If the script reports an incompatible dependency layout, do not force the patch. Review [LoLa dependency workaround](LOLA_DEPENDENCY_FIX.md).

### 9.3 Verify the corrections

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway

G="$(bazel info output_base)/external/score_communication+/score/mw/com/impl/bindings/lola"

grep -nF "return event_data_raw_array;" "$G/skeleton_memory_manager.cpp"

grep -nF "OpenEventDataFromOpenedSharedMemory<std::max_align_t>" "$G/skeleton.cpp"

grep -nF "static_cast<void*>(event_data_storage.data())" "$G/skeleton.cpp"
'
```

All three searches should find their corresponding source statements.

The patch modifies Bazel's local external dependency tree. These changes are not automatically transferred to another machine by cloning the repository.

## 10. Build the Patched Middleware

Rebuild the gateway:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway
bazel build //src/gatewayd:gatewayd_example
'
```

Build `someipd`:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway
bazel build //src/someipd:someipd_example
'
```

Build the LoLa IPC verifier:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway
bazel build //examples/lidar_visualizer_sim:lidar_ipc_verifier
'
```

Expected for each command:

```text
Build completed successfully
```

Compiler deprecation warnings may appear without causing the build to fail.

## 11. Prepare the SOME/IP Binary Configuration

The SOME/IP gateway configuration is defined using:

```text
src/config/mw_someip_config.json
src/config/mw_someip_config.fbs
```

The runtime expects the generated binary configuration:

```text
src/config/mw_someip_config.bin
```

Check the Bazel-generated file:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway
ls -lh bazel-bin/src/config/mw_someip_config.bin
'
```

If available, copy it to the runtime location:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway

cp -L bazel-bin/src/config/mw_someip_config.bin \
  src/config/mw_someip_config.bin
'
```

Verify:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway

ls -lh src/config/mw_someip_config.bin
sha256sum \
  src/config/mw_someip_config.bin \
  bazel-bin/src/config/mw_someip_config.bin
'
```

The SHA-256 hashes should match.

The generated file must correspond to the current source configuration. A successful copy does not independently validate the contents.

If the Bazel-generated file is missing, investigate the configuration-generation target before continuing. Do not copy a binary from a different repository revision.

## 12. Start the Communication Services

Open four Linux terminals.

Each terminal executes one application. Keep each application running while starting the following one.

The startup sequence used in the manual setup is:

```text
1. gatewayd
2. someipd
3. lidar_ipc_verifier
4. lidar_csv_publisher
```

### Terminal 1 — Start gatewayd

```bash
cd ~/work/publisher-score

docker compose exec middleware-node bash -lc \
'cd /app/inc_someip_gateway && bazel run //src/gatewayd:gatewayd_example'
```

The gateway creates the local IPC communication endpoint required by `someipd`.

Keep the terminal running.

### Terminal 2 — Start someipd

```bash
cd ~/work/publisher-score

docker compose exec middleware-node bash -lc \
'cd /app/inc_someip_gateway && unset LD_LIBRARY_PATH && bazel run //src/someipd:someipd_example'
```

Expected output:

```text
[someipd] Starting routing loop...
[someipd] Starting network stack processing...
```

These messages indicate that `someipd` has entered its routing and network-processing loops.

The following logging warning may appear:

```text
No logging configuration files could be found.
Fallback to console logging.
```

This warning alone is not evidence of a communication failure.

If `someipd` exits with code `255`, see the troubleshooting section before continuing.

### Terminal 3 — Start the LoLa IPC verifier

```bash
cd ~/work/publisher-score

docker compose exec middleware-node bash -lc \
'cd /app/inc_someip_gateway && bazel run //examples/lidar_visualizer_sim:lidar_ipc_verifier'
```

Expected initialization messages:

```text
[ipc-verifier] service found
[ipc-verifier] GenericProxy event found
[ipc-verifier] subscribed to lidar_frame
```

These messages confirm that the verifier located and subscribed to the LoLa event. They do not yet confirm payload reception.

### Terminal 4 — Start the LiDAR publisher

```bash
cd ~/work/publisher-score

docker compose exec publisher-node bash -lc \
'cd /app/publisher-node && bash scripts/lidar-node-publisher.sh'
```

The publisher script:

1. Configures the vSomeIP runtime environment.
2. Adds the publisher multicast route when necessary.
3. Builds the C++ LiDAR publisher using CMake.
4. Loads the LiDAR CSV dataset.
5. Transmits frames as SOME/IP events.

The publisher input is:

```text
publisher/data/lidar_data.csv
```

For each frame, the publisher generates a 1020-byte application payload containing 36 LiDAR rays.

## 13. Verify End-to-End Communication

The validation must examine both the gateway and LoLa verifier.

### 13.1 Gateway reception

In the `gatewayd` terminal, look for:

```text
Received SOME/IP event: event=0x8778 payload=1020B
Forwarded event 0x8778 to IPC subscribers
```

These messages indicate that the gateway receives the LiDAR SOME/IP event and attempts to forward it to the IPC subscribers.

### 13.2 LoLa IPC reception

In the verifier terminal, look for:

```text
candidate LiDAR payload offset=16
candidate ray_count=36
PASS: found possible 1020-byte LiDAR payload at offset 16
```

This is the expected verification result.

The test confirms that the payload matching the expected LiDAR frame structure has reached the S-CORE IPC subscriber.

It does not yet establish complete payload integrity, zero frame loss or outbound IPC → SOME/IP forwarding.

## 14. Optional SOME/IP Network Capture

If the publisher reports that it is transmitting frames but the gateway receives nothing, inspect the network traffic.

Open an additional terminal:

```bash
cd ~/work/publisher-score

docker compose exec middleware-node bash -lc \
'tcpdump -ni eth0 "udp port 30490 or udp port 30509"'
```

The capture monitors:

| Port | Purpose |
|---|---|
| UDP `30490` | SOME/IP Service Discovery |
| UDP `30509` | LiDAR SOME/IP events |

Service Discovery traffic may appear as:

```text
172.31.0.10.30490 > 224.224.224.245.30490
```

Receiving Service Discovery traffic does not guarantee that the middleware successfully subscribes to the LiDAR eventgroup.

If only Service Discovery packets appear, investigate service discovery, subscription state and the publisher's event transmission.

If event packets arrive but `gatewayd` does not report reception, investigate `someipd` and its gateway IPC connection.

## 15. Troubleshooting

### 15.1 someipd exits with code 255

Possible symptoms:

```text
Build completed successfully
No logging configuration files could be found.
Fallback to console logging.
SOMEIPD_EXIT_CODE=255
```

The failure occurs during runtime initialization rather than compilation.

The current implementation calls `SomeipMessageTransferProxy::FindService()` for:

```text
someipd/gatewayd_messages
```

before entering the routing loop.

Starting `gatewayd` first allowed `someipd` to initialize in the tested environment.

Check whether the gateway is still running:

```bash
docker compose exec middleware-node bash -lc \
'ps -ef | grep -E "[g]atewayd_example|[s]omeipd_example" || true'
```

Also verify both runtime configuration files are available:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway

ls -lh src/config/mw_someip_config.bin
ls -lh src/someipd/etc/mw_com_config.json
'
```

Exit code `255` alone does not prove that service discovery is the cause. If it persists, inspect the daemon initialization logs and source code before modifying dependencies.

### 15.2 LoLa service-instance lock error

Possible message:

```text
Flock try_lock failed:
Another Skeleton could have already flocked the markerfile
```

This can happen when a previous service process remains active and attempts to offer the same service instance.

Check:

```bash
docker compose exec middleware-node bash -lc \
'ps -ef | grep -E "[s]omeipd_example|[g]atewayd_example|[l]idar_ipc_verifier" || true'
```

Stop unwanted duplicate processes before restarting the services.

Do not indiscriminately remove LoLa shared-memory objects.

### 15.3 Publisher transmits but gateway receives nothing

Check:

```bash
docker compose exec publisher-node ping -c 3 172.31.0.20
```

Verify routing:

```bash
docker compose exec publisher-node ip route
docker compose exec middleware-node ip route
```

Verify that `someipd` is running:

```bash
docker compose exec middleware-node bash -lc \
'ps -ef | grep "[s]omeipd_example"'
```

Capture Service Discovery and LiDAR event traffic using `tcpdump`.

Check the configured service and instance identifiers on both sides.

### 15.4 Gateway receives frames but IPC verifier does not

Confirm that the LoLa corrections were applied to the active Bazel dependency and that `gatewayd` was rebuilt afterward.

Check that the verifier has discovered and subscribed to the correct LoLa service.

If necessary, consult:

[LoLa dependency workaround](LOLA_DEPENDENCY_FIX.md)

### 15.5 Bazel dependency path is missing

If the LoLa source cannot be found, locate Bazel's active external dependency directory:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway

bazel info output_base

find "$(bazel info output_base)/external" \
  -name skeleton_memory_manager.cpp 2>/dev/null
'
```

Build the gateway first if the dependency has not been fetched.

### 15.6 Docker network subnet conflict

Check existing networks:

```bash
docker network ls
```

Inspect subnet assignments before creating or deleting networks.

Avoid removing networks belonging to unrelated Docker projects.

## 16. Stop the Test

Stop the publisher, IPC verifier, `someipd` and `gatewayd` using Ctrl+C in their respective terminals.

Check for remaining processes:

```bash
docker compose exec middleware-node bash -lc \
'ps -ef | grep -E "[s]omeipd_example|[g]atewayd_example|[l]idar_ipc_verifier" || true'
```

Check container status:

```bash
docker compose ps
```

To stop and remove the project containers:

```bash
docker compose down
```

The external `sdv-net` network is preserved.

Remember that recreating the middleware container can remove manually installed container packages and resets its manually added multicast route. Bazel caches stored only inside the container can also be lost.

## 17. Test Acceptance Criteria

The setup is considered successful at the structural reception level when the following conditions are met:

- Both Docker containers are running with the expected IP addresses.
- Network connectivity between containers is verified.
- The LoLa dependency patch is applied.
- The middleware targets build successfully.
- `gatewayd` and `someipd` initialize.
- The IPC verifier subscribes to the configured LiDAR event.
- The publisher transmits the expected LiDAR frames.
- The gateway reports receiving and forwarding 1020-byte events.
- The IPC verifier reports:

```text
PASS: found possible 1020-byte LiDAR payload at offset 16
```

## 18. Current Scope and Planned Extension

The implemented inbound path is:

```text
External LiDAR Publisher
        |
        | SOME/IP
        v
      someipd
        |
      gatewayd
        |
        | LoLa IPC
        v
  lidar_ipc_verifier
```

The planned outbound path is:

```text
S-CORE LiDAR Microservice
        |
        | mw::com / LoLa IPC
        v
      gatewayd
        |
      someipd
        |
        | SOME/IP
        v
External Subscriber Container
```

Future work includes full payload-integrity verification, frame-loss measurements, latency measurements and outbound communication testing.

The present test establishes structural reception of the expected LiDAR payload through the inbound SOME/IP–LoLa communication path.

## References

- [Project README](../README.md)
- [Architecture and testing](ARCHITECTURE_AND_TESTING.md)
- [LoLa dependency workaround](LOLA_DEPENDENCY_FIX.md)
- [Troubleshooting](TROUBLESHOOTING.md)
- [Development roadmap](DEVELOPMENT.md)
