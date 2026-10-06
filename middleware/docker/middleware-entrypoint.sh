#!/usr/bin/env bash
set -e

echo "[middleware-entrypoint] cleaning stale vSomeIP sockets"
rm -f /tmp/vsomeip-*

echo "[middleware-entrypoint] ensuring multicast route"
if ! ip route show 224.0.0.0/4 | grep -q "224.0.0.0/4"; then
  ip route add 224.0.0.0/4 dev eth0
fi

echo "[middleware-entrypoint] current routes:"
ip route show

exec "$@"
