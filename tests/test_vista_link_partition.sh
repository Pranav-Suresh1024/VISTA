#!/usr/bin/env bash
set -euo pipefail
binary=./build/vista
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
for run in first second; do
    code=0
    "$binary" examples/vista_link/all_type_routes.workflow.vista --emit partitions > "$tmp/$run" || code=$?
    test "$code" = 1
    grep -q 'Status: UNSUPPORTED' "$tmp/$run"
    grep -q CERT003 "$tmp/$run"
done
cmp "$tmp/first" "$tmp/second"
bash tests/create_vista_link_groups.sh "$tmp"
for group in numeric textual selection; do
    directory="$tmp/$group-source"
    for run in first second; do
        "$binary" "$directory/all_type_routes.workflow.vista" --emit partitions > "$tmp/$group-$run"
        grep -q 'Status: COMPLETE' "$tmp/$group-$run"
    done
    cmp "$tmp/$group-first" "$tmp/$group-second"
    printf '%s: ' "$group"; grep '^Cases:' "$tmp/$group-first"
done
make --no-print-directory build/test-partition-unit
./build/test-partition-unit
if "$binary" examples/vista_link/conditional_cycle.workflow.vista --emit partitions > "$tmp/cycle" 2>&1; then
    echo 'FAIL: conditional cycle reported complete'; exit 1
fi
grep -q CERT002 "$tmp/cycle"
echo 'VISTA-Link exact partition and reachable-case tests passed.'
