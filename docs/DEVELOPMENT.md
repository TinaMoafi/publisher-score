# Development Roadmap and Contribution Notes

[Overview](../README.md) · [Architecture](ARCHITECTURE_AND_TESTING.md) · [Setup](DOCKER_SETUP.md)

## Current Implementation

The existing implementation demonstrates inbound communication between an external LiDAR publisher and Eclipse S-CORE middleware.

```text
External LiDAR Publisher
        |
        | SOME/IP
        v
      someipd
        |
        | Gateway IPC
        v
      gatewayd
        |
        | mw::com / LoLa
        v
  lidar_ipc_verifier
```

The validation confirms that a 1020-byte LiDAR payload containing 36 rays reaches the LoLa IPC verifier and is structurally recognized.

This implementation serves as the baseline for subsequent development and regression testing.

## Target Architecture

The next development phase focuses on implementing the reverse communication direction: publishing data from an S-CORE application to an external SOME/IP subscriber.

```text
S-CORE Environment
+------------------------------------+
|                                    |
| Custom LiDAR Microservice          |
|         |                          |
|         | mw::com Skeleton         |
|         v                          |
|     LoLa Shared Memory             |
|         |                          |
|         v                          |
|      gatewayd                      |
|         |                          |
|         | Gateway IPC              |
|         v                          |
|      someipd                       |
|         |                          |
+---------|--------------------------+
          |
          | SOME/IP over Ethernet
          v
External Subscriber Container
+------------------------------------+
|                                    |
| C++ vSomeIP Subscriber             |
|         |                          |
|         v                          |
| LiDAR Frame Reception              |
|         |                          |
|         v                          |
| Payload Validation                 |
|                                    |
+------------------------------------+
```

The S-CORE microservice will publish LiDAR events using `mw::com`/LoLa IPC rather than directly using vSomeIP.

The gateway will bridge the local IPC service to the external SOME/IP network.

## Planned Development

### Phase 1 — Preserve the Inbound Baseline

Maintain the existing LiDAR publisher-to-IPC integration as a regression test.

- Preserve the current service and event configuration.
- Keep the manual LoLa dependency correction procedure.
- Verify successful reception of 1020-byte LiDAR frames.
- Retain representative gateway and verifier logs.

### Phase 2 — Implement an S-CORE LiDAR Microservice

Develop a C++ application that generates or reads LiDAR frames and publishes them through S-CORE `mw::com`.

The microservice will:

- Initialize the S-CORE communication runtime.
- Create and offer a local service using an `mw::com` Skeleton.
- Publish LiDAR frames through a LoLa event.
- Manage sample buffers and event publication.
- Support controlled frame generation for integration testing.

Initially, the microservice should use the existing LiDAR payload format to simplify comparison with the inbound implementation.

### Phase 3 — Configure Outbound Gateway Forwarding

Integrate the local S-CORE service with `gatewayd` and `someipd`.

The implementation requires:

- Defining the local service type, instance and event.
- Configuring the required S-CORE service-instance manifests.
- Connecting `gatewayd` to the locally offered IPC service.
- Mapping the local event to an outgoing SOME/IP event.
- Configuring `someipd` to expose the corresponding SOME/IP service.

The outgoing configuration must use consistent service, instance and event identifiers across the S-CORE and SOME/IP components.

The existing upstream `LocalServiceInstance` implementation is a reference for local-service forwarding.

### Phase 4 — Implement an External SOME/IP Subscriber

Develop a separate Docker container containing a C++ vSomeIP subscriber.

The subscriber will:

- Initialize the vSomeIP runtime.
- Discover the configured SOME/IP service.
- Subscribe to the LiDAR eventgroup.
- Receive and deserialize LiDAR frames.
- Record frame timestamps, payload sizes and frame counts.
- Report successful reception and validation results.

### Phase 5 — End-to-End Validation

Verify the complete outgoing communication path:

```text
LiDAR Microservice
       |
       v
   LoLa IPC
       |
       v
    gatewayd
       |
       v
    someipd
       |
       v
SOME/IP Network
       |
       v
External Subscriber
```

Validation will include:

1. Successful S-CORE service initialization.
2. Local IPC service discovery and event publication.
3. Gateway reception and forwarding.
4. SOME/IP service discovery and event subscription.
5. External subscriber reception.
6. Application-payload integrity.
7. Frame-loss and ordering measurements.
8. End-to-end latency measurements.

## Payload and Interface Compatibility

The current LiDAR application payload contains:

| Field | Size |
|---|---:|
| Ray count | 4 bytes |
| Frame timestamp | 8 bytes |
| 36 LiDAR ray records | 1008 bytes |
| **Total** | **1020 bytes** |

Each ray contains a timestamp, ray identifier, X/Y/Z coordinates and intensity.

For cross-platform interoperability, the outgoing implementation should define an explicit serialization format, including field sizes and byte order.

The outgoing service identifiers must be selected consistently and must not create unintended loops with the existing inbound service.

## Dependency and Build Management

The existing project uses `score_communication` version `0.2.1`, with three manually applied LoLa corrections.

The manual patch is retained for the current implementation.

Future dependency improvements may include integrating a version-pinned patch into Bazel or adopting an upstream release containing the required corrections.

Any dependency changes must be validated using a clean build and the established inbound regression test before replacing the working configuration.

## Change Checklist

When modifying communication interfaces, update the corresponding application code, service definitions, SOME/IP JSON configurations, S-CORE manifests and architecture documentation.

When modifying Docker deployment, maintain consistent container networking and distinguish the repository-root Compose configuration from the separate middleware-only Compose setup.

When modifying dependencies, record the dependency version, patch state and build results.

## Evidence and Contribution Requirements

For each significant implementation or integration change, record:

- Git revision and relevant source changes.
- Host and container environments.
- Docker and Bazel versions.
- Service and event configurations.
- Dependency versions and patch state.
- Build and execution commands.
- Gateway and application logs.
- Test outcomes and remaining limitations.

Distinguish implemented and validated functionality from planned features.

## Development Objective

The final objective is to demonstrate interoperable communication between a native S-CORE microservice and an external software component using LoLa IPC internally and SOME/IP externally.

The existing inbound LiDAR integration remains the reference test, while the outbound microservice-to-subscriber path is the next implementation target.
