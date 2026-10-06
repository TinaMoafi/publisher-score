#!/usr/bin/env bash
set -euo pipefail

cd /app/publisher-node

export VSOMEIP_CONFIGURATION=/app/publisher-node/config/vsomeip/publisher-container.json
export LD_LIBRARY_PATH=/usr/local/lib:${LD_LIBRARY_PATH:-}

rm -rf /tmp/vsomeip-*

ip route | grep -q "224.0.0.0/4" || ip route add 224.0.0.0/4 dev eth0 || true

cmake -S publisher -B build
cmake --build build -j2

./build/lidar_csv_publisher