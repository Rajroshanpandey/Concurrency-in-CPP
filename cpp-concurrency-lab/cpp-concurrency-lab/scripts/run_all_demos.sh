#!/usr/bin/env bash
# Runs every demo in sequence (skips the intentionally unsafe one). Handy for a full tour or CI logs.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/${LAB_PRESET:-default}/bin"
fail=0
for d in $(ls "$BIN" | grep -E '^(s[0-9]{2}_|mini_ecu)' | sort); do
  [[ "$d" == s10_tough_uaf_pop ]] && continue
  "$BIN/$d" || { echo ">>> $d exited with $?"; fail=$((fail+1)); }
done
echo "demos with non-zero exit: $fail"
exit $fail
