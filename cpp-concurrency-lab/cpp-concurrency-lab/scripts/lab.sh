#!/usr/bin/env bash
# lab.sh - the front door of the lab: build, browse, run, test.
#   ./scripts/lab.sh                  interactive menu
#   ./scripts/lab.sh build            configure + build (preset from $LAB_PRESET, default: default)
#   ./scripts/lab.sh list             list every demo
#   ./scripts/lab.sh run <name|prefix> [args...]
#   ./scripts/lab.sh test             run ctest for the preset
#   LAB_PRESET=tsan ./scripts/lab.sh build     (presets: default | tsan | asan | release)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRESET="${LAB_PRESET:-default}"
BIN="$ROOT/build/$PRESET/bin"

if [[ -t 1 && -z "${NO_COLOR:-}" ]]; then
  B=$'\033[1m'; D=$'\033[2m'; R=$'\033[0m'; G=$'\033[1;32m'; Y=$'\033[1;33m'; RD=$'\033[1;31m'; C=$'\033[1;36m'
else B=; D=; R=; G=; Y=; RD=; C=; fi

declare -A TITLE=(
  [01]="std::thread, join, detach"      [02]="Race conditions"
  [03]="Mutexes & deadlocks"            [04]="Condition variables"
  [05]="std::atomic"                    [06]="Memory ordering"
  [07]="Futures, promises, async"       [08]="Thread pools"
  [09]="jthread & stop_token"           [10]="Lock-free programming"
  [11]="Real-time scheduling"           [12]="CAPSTONE: Mini ECU"
)

need_build() {
  if [[ ! -d "$BIN" ]]; then
    echo "${Y}No build found for preset '$PRESET'. Building first...${R}"
    cmd_build
  fi
}

cmd_build() {
  cd "$ROOT"
  cmake --preset "$PRESET"
  cmake --build --preset "$PRESET" -j "$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
  echo "${G}Build OK${R} -> $BIN"
}

demos() {
  ls "$BIN" 2>/dev/null | grep -E '^s[0-9]{2}_' | sort
  [[ -x "$BIN/mini_ecu" ]] && echo mini_ecu
  return 0
}
stage_of() { [[ "$1" == mini_ecu ]] && echo 12 || echo "${1:1:2}"; }
tier_of() {
  case "$1" in
    *_easy_*)   printf '%s' "${G}EASY  ${R}";;
    *_medium_*) printf '%s' "${Y}MEDIUM${R}";;
    *_tough_*)  printf '%s' "${RD}TOUGH ${R}";;
    *)          printf '%s' "${C}FINAL ${R}";;
  esac
}

cmd_list() {
  need_build
  local last=""
  while read -r d; do
    local s; s="$(stage_of "$d")"
    if [[ "$s" != "$last" ]]; then echo; echo "${B}Stage $s - ${TITLE[$s]}${R}"; last="$s"; fi
    echo "   $(tier_of "$d")  $d"
  done < <(demos)
  echo
}

resolve() {
  local matches; matches="$(demos | grep -E "^$1" || true)"
  local n; n="$(grep -c . <<<"$matches" || true)"
  if [[ -z "$matches" ]]; then echo "${RD}no demo matches '$1'${R}" >&2; return 1; fi
  if [[ "$n" -gt 1 ]]; then
    local exact; exact="$(grep -x "$1" <<<"$matches" || true)"
    [[ -n "$exact" ]] && { echo "$exact"; return; }
    echo "${RD}'$1' is ambiguous:${R}" >&2; echo "$matches" >&2; return 1
  fi
  echo "$matches"
}

cmd_run() {
  need_build
  local name; name="$(resolve "$1")"; shift
  echo "${D}\$ $BIN/$name $*${R}"
  "$BIN/$name" "$@"
}

cmd_test() { need_build; cd "$ROOT"; ctest --preset "$PRESET"; }

cmd_menu() {
  need_build
  while true; do
    clear 2>/dev/null || true
    echo "${C}  ============================================${R}"
    echo "${C}   C++ CONCURRENCY LAB  -  zero to real-time${R}"
    echo "${C}  ============================================${R}"
    echo "${D}  preset: $PRESET   (change with LAB_PRESET=tsan|asan|release)${R}"
    echo
    for s in 01 02 03 04 05 06 07 08 09 10 11 12; do printf "   %s  %s\n" "${B}$s${R}" "${TITLE[$s]}"; done
    echo "   ${B}t${R}   run all tests      ${B}q${R}   quit"
    echo
    read -r -p "  choose a stage > " choice || exit 0
    case "$choice" in
      q|Q) exit 0;;
      t|T) cmd_test || true; read -r -p "  [enter] " _;;
      0[1-9]|1[0-2]|[1-9])
        [[ ${#choice} -eq 1 ]] && choice="0$choice"
        stage_menu "$choice";;
    esac
  done
}

stage_menu() {
  local s="$1"
  while true; do
    clear 2>/dev/null || true
    echo "${B}Stage $s - ${TITLE[$s]}${R}   ${D}(theory: stages/*$s*/README.md)${R}"
    echo
    mapfile -t list < <(demos | while read -r d; do [[ "$(stage_of "$d")" == "$s" ]] && echo "$d"; done)
    local i=1
    for d in "${list[@]}"; do printf "   %s  %s  %s\n" "${B}$i${R}" "$(tier_of "$d")" "$d"; i=$((i+1)); done
    echo "   ${B}b${R}   back"
    echo
    read -r -p "  run which demo? > " pick || return
    [[ "$pick" == b || "$pick" == B || -z "$pick" ]] && return
    if [[ "$pick" =~ ^[0-9]+$ ]] && (( pick >= 1 && pick <= ${#list[@]} )); then
      local name="${list[$((pick-1))]}"
      read -r -p "  extra arguments (optional) > " extra || true
      clear 2>/dev/null || true
      # shellcheck disable=SC2086
      "$BIN/$name" $extra || echo "${RD}exit code $?${R}"
      echo; read -r -p "  [enter] to go back " _
    fi
  done
}

case "${1:-menu}" in
  build) cmd_build;;
  list)  cmd_list;;
  run)   shift; [[ $# -ge 1 ]] || { echo "usage: lab.sh run <name> [args]"; exit 1; }; cmd_run "$@";;
  test)  cmd_test;;
  menu)  cmd_menu;;
  *) sed -n '2,10p' "$0"; exit 1;;
esac
