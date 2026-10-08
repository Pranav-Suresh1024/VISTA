#!/usr/bin/env bash
set -euo pipefail
root=$1
# ponytail: current fixture fields occupy one line; revise if their syntax changes.
for group in numeric textual selection; do
    case "$group" in
        numeric) types='integer|decimal|date|boolean';;
        textual) types='text|textarea|email|phone|boolean';;
        selection) types='choice|file|boolean';;
    esac
    directory="$root/$group-source"
    mkdir "$directory"
    cp examples/vista_link/{all_type_routes.workflow,applicant}.vista "$directory/"
    awk -v types="$types" '$1 != "field" || $3 ~ ("^(" types ")$")' examples/vista_link/all_type_routes.vista > "$directory/all_type_routes.vista"
done
grep -h '^  field ' "$root/"{numeric,textual,selection}-source/all_type_routes.vista | sort -u > "$root/group-fields"
grep '^  field ' examples/vista_link/all_type_routes.vista | sort > "$root/original-fields"
cmp "$root/original-fields" "$root/group-fields"
