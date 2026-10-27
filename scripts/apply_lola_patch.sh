#!/usr/bin/env bash
set -euo pipefail

# Run from /app/inc_someip_gateway inside middleware-node, after initial Bazel fetch.
WORKSPACE="${PWD}"
MODULE="${WORKSPACE}/MODULE.bazel"
EXTERNAL="${WORKSPACE}/bazel-inc_someip_gateway/external/score_communication+"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATCH="${SCRIPT_DIR}/score_communication_lola_generic_buffer.patch"

[[ -f "$MODULE" ]] || { echo "ERROR: run from the Bazel workspace root" >&2; exit 1; }
grep -Eq 'bazel_dep\(name = "score_communication", version = "0\.2\.1"\)' "$MODULE" || {
  echo "ERROR: dependency version differs from 0.2.1; review patch manually" >&2; exit 1;
}
[[ -d "$EXTERNAL" ]] || { echo "ERROR: Bazel external dependency absent. Fetch/build first: bazel build //src/gatewayd:gatewayd_example" >&2; exit 1; }
[[ -f "$PATCH" ]] || { echo "ERROR: missing patch: $PATCH" >&2; exit 1; }
if (cd "$EXTERNAL" && git apply --check "$PATCH"); then
  (cd "$EXTERNAL" && git apply "$PATCH")
  echo "Applied LoLa patch. Rebuild gatewayd and run the verifier."
elif (cd "$EXTERNAL" && git apply --reverse --check "$PATCH"); then
  echo "LoLa patch already applied."
else
  echo "ERROR: patch does not match dependency contents; stopped without modifying files" >&2
  exit 1
fi
