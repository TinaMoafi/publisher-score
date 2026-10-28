**Verified implementation:** The inbound communication path delivers simulated LiDAR frames from an external vSomeIP publisher through `someipd` and `gatewayd` into S-CORE LoLa IPC. The `lidar_ipc_verifier` successfully recognizes a 1020-byte application payload containing 36 LiDAR rays at sample offset 16. This verifies structural reception, not byte-for-byte payload integrity.

**Status:** Inbound SOME/IP → LoLa IPC communication is implemented and verified. IPC → SOME/IP forwarding and an external subscriber are planned extensions. The `score_communication` 0.2.1 dependency requires three local LoLa corrections, provided through the manual patch procedure described in [Dependency fix](docs/LOLA_DEPENDENCY_FIX.md).

## Architecture

```text
publisher-node (172.31.0.10)                middleware-node (172.31.0.20)

CSV -> LiDAR C++ Publisher -- SOME/IP --> someipd -> gatewayd -- LoLa IPC --> IPC Verifier

Service: 0x1234
Instance: 0x5678
Event/Eventgroup: 0x8778
```

**Planned outbound architecture:**

```text
S-CORE LiDAR Microservice
          |
          | mw::com / LoLa IPC
          v
       gatewayd
          |
          | Gateway IPC
          v
        someipd
          |
          | SOME/IP over Ethernet
          v
External Subscriber Container
```
