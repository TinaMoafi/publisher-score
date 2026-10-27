
# LoLa generic sample-buffer workaround (`score_communication` 0.2.1)

[Overview](../README.md) · [Setup](DOCKER_SETUP.md) · [Troubleshooting](TROUBLESHOOTING.md)

## Symptom and root-cause investigation

On a new Debian-hosted Docker environment, `gatewayd` received incoming `0x8778` events with `payload=1020B`, but `lidar_ipc_verifier` reported `FAIL: ray_count=36 not found in sample storage`. Prior investigation found a producer/consumer sample-address difference of 72 bytes. The relevant generic-event LoLa code returned a `DynamicArray` object address rather than the address of the element data buffer. The reopening path also instantiated `EventDataStorage<std::uint8_t>` while creation used `EventDataStorage<std::max_align_t>`.

**Evidence:** After applying all three source edits, rebuilding `gatewayd`, stopping older processes, cleaning unreferenced stale LoLa objects and starting `someipd` before `gatewayd`, the verifier reported `PASS: found possible 1020-byte LiDAR payload at offset 16` and `ray_count=36`. The test does not isolate which of the three edits are independently necessary.

## Dependency and patch scope

`middleware/inc_someip_gateway/MODULE.bazel` declares `bazel_dep(name = "score_communication", version = "0.2.1")`. The relevant generated external path in the tested container was:

```text
/app/inc_someip_gateway/bazel-inc_someip_gateway/external/score_communication+/
```

These files are in Bazel's **generated external tree**, outside the repository's tracked application source. Editing them fixes the running local environment, not a GitHub clone. The repository includes a **proposed, version-specific** unified diff at `patches/score_communication_lola_generic_buffer.patch` and a guarded helper at `scripts/apply_lola_patch.sh`. This patch has **not been validated against a pristine fetched archive**; always check it before depending on it.

## Exact changes

In `score/mw/com/impl/bindings/lola/skeleton_memory_manager.cpp`, `SkeletonMemoryManager::CreateGenericEventDataInCreatedSharedMemory()`:

```diff
-    return data_storage;
+    return event_data_raw_array;
```

In `score/mw/com/impl/bindings/lola/skeleton.cpp`, `Skeleton::RegisterGeneric()` reopened-memory branch:

```diff
-        auto& event_data_storage = memory_manager_.OpenEventDataFromOpenedSharedMemory<std::uint8_t>(element_fq_id);
+        auto& event_data_storage = memory_manager_.OpenEventDataFromOpenedSharedMemory<std::max_align_t>(element_fq_id);
 
-        return {static_cast<void*>(&event_data_storage), event_data_control_composite};
+        return {static_cast<void*>(event_data_storage.data()), event_data_control_composite};
```

Creation explicitly allocates `EventDataStorage<std::max_align_t>` and calculates a buffer length rounded to a number of `std::max_align_t` elements; the reopened branch must interpret the same object specialization. The container's `.data()` pointer is used as the sample base.

## Applying the bundled patch

From the host repo root, after starting containers and fetching the Bazel dependency with an initial build:

```bash
mkdir -p middleware/inc_someip_gateway/local_lola_patch
cp patches/score_communication_lola_generic_buffer.patch scripts/apply_lola_patch.sh middleware/inc_someip_gateway/local_lola_patch/
docker compose exec middleware-node bash -lc 'cd /app/inc_someip_gateway && bash local_lola_patch/apply_lola_patch.sh'
docker compose exec middleware-node bash -lc 'cd /app/inc_someip_gateway && bazel build //src/gatewayd:gatewayd_example'
```

The script finds the external dependency through the Bazel workspace symlink, checks all three expected original fragments, and uses `git apply --check` before modifying files. It detects an already-patched state and refuses unknown variants. **Never force-apply a patch if it fails.** Confirm the local `MODULE.bazel` still pins 0.2.1 and inspect the upstream changes.

## Verify manually

```bash
cd /app/inc_someip_gateway
G=bazel-inc_someip_gateway/external/score_communication+/score/mw/com/impl/bindings/lola
grep -n 'return event_data_raw_array;' "$G/skeleton_memory_manager.cpp"
grep -n -A 3 'OpenEventDataFromOpenedSharedMemory<std::max_align_t>' "$G/skeleton.cpp"
```

Expected `return event_data_raw_array;` and `return {static_cast<void*>(event_data_storage.data()), event_data_control_composite};`. Build success is not enough; rerun the publisher/gateway/verifier test per [Architecture and testing](ARCHITECTURE_AND_TESTING.md).

## Permanent resolution (outstanding)

Bazel already uses `single_version_override(... patches = [...], patch_strip = 1)` for another module in this project. An appropriate long-term implementation is a **tested** `single_version_override` for `score_communication` 0.2.1 with a committed patch label such as `//:third_party/score_communication_lola_generic_buffer.patch`. This requires placing the patch under the Bazel module, checking its strip level and verifying a *clean dependency fetch* builds successfully. Do not add that override blindly; first validate patch application against the pristine archive and run both build and runtime tests. Alternatively, use an upstream fixed release and remove the workaround.

Reapplying modifications directly in `bazel-*/external` is a temporary procedure; `bazel clean --expunge` or changes to the Bazel cache may discard them. Track the patch and resolve its build integration before declaring the repo turnkey.
