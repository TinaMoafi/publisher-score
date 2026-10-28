# LoLa Generic Sample-Buffer Workaround (`score_communication` 0.2.1)

[Overview](../README.md) · [Setup](DOCKER_SETUP.md) · [Troubleshooting](TROUBLESHOOTING.md)

## 1. Problem Description

The LiDAR SOME/IP–LoLa integration requires a correction in the generic-event shared-memory handling of `score_communication` version `0.2.1`.

During testing, `gatewayd` successfully received incoming SOME/IP events with:

```text
event=0x8778 payload=1020B
```

However, the LoLa IPC verifier initially reported:

```text
FAIL: ray_count=36 not found in sample storage
```

Investigation identified a difference of 72 bytes between the producer and consumer sample addresses.

The generic-event implementation returned the address of a `DynamicArray` object instead of the underlying element-data buffer.

A second inconsistency was identified when reopening shared memory: the implementation used `EventDataStorage<std::uint8_t>`, while the creation path used `EventDataStorage<std::max_align_t>`.

These issues affected the interpretation of the generic-event sample storage.

## 2. Implemented Workaround

Three source-level corrections were applied to the LoLa binding implementation.

After applying the corrections, rebuilding `gatewayd`, and restarting the middleware applications with a clean runtime state, the verifier successfully recognized the expected LiDAR payload:

```text
candidate LiDAR payload offset=16
candidate ray_count=36
PASS: found possible 1020-byte LiDAR payload at offset 16
```

The result demonstrates structural payload recognition. It does not independently establish which of the three corrections is necessary or sufficient.

## 3. Dependency and Patch Scope

The project declares the following dependency in:

```text
middleware/inc_someip_gateway/MODULE.bazel
```

```python
bazel_dep(name = "score_communication", version = "0.2.1")
```

Bazel downloads the dependency into its external dependency directory.

The exact directory depends on the active Bazel output base.

To locate it, execute inside `middleware-node`:

```bash
cd /app/inc_someip_gateway
bazel info output_base
```

The dependency is normally located at:

```text
<output_base>/external/score_communication+/
```

For example:

```text
/root/.cache/bazel/_bazel_root/<workspace-hash>/external/score_communication+/
```

These files belong to Bazel's generated external dependency tree and are not tracked as part of the application's Git repository.

Therefore, changes made directly to this directory affect only the current Bazel environment.

The repository provides:

```text
patches/score_communication_lola_generic_buffer.patch
scripts/apply_lola_patch.sh
```

The patch is version-specific and applied manually. It is not automatically integrated into Bazel dependency resolution.

## 4. Source Corrections

### Correction 1 — Generic-event data-buffer address

File:

```text
score/mw/com/impl/bindings/lola/skeleton_memory_manager.cpp
```

Function:

```cpp
SkeletonMemoryManager::CreateGenericEventDataInCreatedSharedMemory()
```

Original:

```cpp
return data_storage;
```

Corrected:

```cpp
return event_data_raw_array;
```

The corrected implementation returns the underlying event-data buffer rather than the storage-container object.

### Correction 2 — Shared-memory storage type

File:

```text
score/mw/com/impl/bindings/lola/skeleton.cpp
```

Function:

```cpp
Skeleton::RegisterGeneric()
```

Original:

```cpp
auto& event_data_storage =
    memory_manager_.OpenEventDataFromOpenedSharedMemory<std::uint8_t>(element_fq_id);
```

Corrected:

```cpp
auto& event_data_storage =
    memory_manager_.OpenEventDataFromOpenedSharedMemory<std::max_align_t>(element_fq_id);
```

The correction aligns the reopening path with the type used when creating the shared-memory storage.

### Correction 3 — Reopened sample-buffer pointer

Original:

```cpp
return {static_cast<void*>(&event_data_storage), event_data_control_composite};
```

Corrected:

```cpp
return {static_cast<void*>(event_data_storage.data()), event_data_control_composite};
```

The corrected implementation returns the address of the stored elements rather than the address of the storage object.

Together, these changes correct the generic-event sample-buffer handling observed in the tested dependency version.

## 5. Manual Patch Procedure

The procedure must be performed after cloning the project, starting Docker containers and fetching the Bazel dependency.

The complete environment setup is described in [Setup and operation](DOCKER_SETUP.md).

### Step 1 — Fetch the dependency

From the Linux host, in the repository root:

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway
bazel build //src/gatewayd:gatewayd_example
'
```

The initial build downloads the dependency and may compile an uncorrected gateway.

Do not use this initial binary for the final communication test.

### Step 2 — Prepare the patch directory

On the Linux host:

```bash
mkdir -p middleware/inc_someip_gateway/local_lola_patch

cp patches/score_communication_lola_generic_buffer.patch \
  middleware/inc_someip_gateway/local_lola_patch/

cp scripts/apply_lola_patch.sh \
  middleware/inc_someip_gateway/local_lola_patch/
```

### Step 3 — Apply the patch

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

If already applied:

```text
LoLa patch already applied.
```

The helper checks patch compatibility before modifying the dependency.

If it reports a mismatch, do not force the patch. Verify the dependency version and inspect the affected source files.

### Step 4 — Rebuild the gateway

```bash
docker compose exec middleware-node bash -lc '
cd /app/inc_someip_gateway
bazel build //src/gatewayd:gatewayd_example
'
```

Expected:

```text
Build completed successfully
```

This rebuild ensures the gateway uses the corrected dependency implementation.

## 6. Verify the Corrections

Enter the middleware container:

```bash
docker compose exec middleware-node bash
```

Locate the dependency:

```bash
cd /app/inc_someip_gateway

G="$(bazel info output_base)/external/score_communication+/score/mw/com/impl/bindings/lola"
```

Verify Correction 1:

```bash
grep -nF "return event_data_raw_array;" "$G/skeleton_memory_manager.cpp"
```

Verify Correction 2:

```bash
grep -nF "OpenEventDataFromOpenedSharedMemory<std::max_align_t>" "$G/skeleton.cpp"
```

Verify Correction 3:

```bash
grep -nF "static_cast<void*>(event_data_storage.data())" "$G/skeleton.cpp"
```

All three commands should find the corresponding corrected statements.

The presence of these statements confirms the source corrections, but a runtime test is still required.

## 7. Runtime Verification

After rebuilding the gateway, run the complete LiDAR communication test described in [Setup and operation](DOCKER_SETUP.md).

The manual startup sequence is:

1. `gatewayd`
2. `someipd`
3. `lidar_ipc_verifier`
4. LiDAR publisher

Confirm that `someipd` enters its routing and network processing loops.

The gateway should report reception and forwarding of 1020-byte LiDAR events.

The verifier should report:

```text
candidate LiDAR payload offset=16
candidate ray_count=36
PASS: found possible 1020-byte LiDAR payload at offset 16
```

This establishes successful structural reception through the inbound SOME/IP–LoLa communication path.

Service initialization and startup-order issues are separate from the generic-event buffer correction. See [Troubleshooting](TROUBLESHOOTING.md).

## 8. Known Limitations

The workaround has the following limitations:

- It targets `score_communication` version `0.2.1`.
- The patch is applied manually to Bazel's external dependency tree.
- Changes to the Bazel cache may remove the corrections.
- `bazel clean --expunge` may require fetching and patching the dependency again.
- A successful build does not guarantee correct IPC runtime behavior.
- Full payload integrity and frame-loss verification remain separate test objectives.

The bundled patch should be checked against the fetched dependency before use. Compatibility with future dependency revisions is not guaranteed.

## 9. Long-Term Resolution

A permanent solution may use a version-pinned Bazel patch override or an upstream release containing the corrected implementation.

Bazel supports dependency patching through mechanisms such as:

```python
single_version_override(
    module_name = "score_communication",
    version = "0.2.1",
    patch_strip = 1,
    patches = [
        "//:third_party/score_communication_lola_generic_buffer.patch",
    ],
)
```

This is a proposed approach, **not part of the currently validated manual setup**.

Before integrating such a change, the patch must be tested against a pristine dependency download, followed by a clean build and the full LiDAR communication regression test.

Until a permanent solution is validated, the manual patch procedure remains the supported workaround for this project.

## 10. Summary

The LoLa corrections address generic-event shared-memory sample-buffer handling in `score_communication` 0.2.1.

The corrected implementation enables the IPC verifier to recognize the expected 1020-byte LiDAR payload containing 36 rays.

The workaround is maintained as a separate patch and helper script to preserve the current gateway source structure while supporting reproducible manual testing.
