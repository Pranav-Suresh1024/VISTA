#!/usr/bin/env bash
set -euo pipefail
binary=./build/vista
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
"$binary" examples/vista_link/all_type_routes.workflow.vista --emit partitions > "$tmp/first"
"$binary" examples/vista_link/all_type_routes.workflow.vista --emit partitions > "$tmp/second"
cmp "$tmp/first" "$tmp/second"
grep -q 'Status: COMPLETE' "$tmp/first"
make --no-print-directory build/test-partition-unit
./build/test-partition-unit
if "$binary" examples/vista_link/conditional_cycle.workflow.vista --emit partitions > "$tmp/cycle" 2>&1; then
    echo 'FAIL: conditional cycle reported complete'; exit 1
fi
grep -q CERT002 "$tmp/cycle"
echo 'VISTA-Link exact partition and reachable-case tests passed.'
