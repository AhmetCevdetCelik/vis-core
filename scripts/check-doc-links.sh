#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
status=0

while IFS= read -r file; do
  dir="$(dirname "$file")"
  while IFS= read -r target; do
    case "$target" in
      http://*|https://*|mailto:*|\#*|"")
        continue
        ;;
    esac

    path="${target%%#*}"
    if [[ -z "$path" ]]; then
      continue
    fi
    if [[ "$path" != *.md && "$path" != *.yml && "$path" != *.yaml ]]; then
      continue
    fi

    if [[ "$path" = /* ]]; then
      resolved="$root${path}"
    else
      resolved="$dir/$path"
    fi
    if [[ ! -f "$resolved" ]]; then
      echo "[docs] broken link in ${file#$root/}: $target"
      status=1
    fi
  done < <(grep -oE '\[[^]]+\]\(([^)]+)\)' "$file" |
           sed -E 's/.*\(([^)]+)\).*/\1/')
done < <(find "$root" \
  -path "$root/.git" -prune -o \
  -path "$root/vis-jitter/out" -prune -o \
  -path "$root/vis-jitter/doctor-bundle-smoke" -prune -o \
  -name '*.md' -type f -print)

if [[ "$status" -eq 0 ]]; then
  echo "[docs] markdown local links ok"
fi
exit "$status"
