#!/usr/bin/env bash
# Build every firmware for the browser and copy it into the homepage:
#
#   web/publish.sh ~/tim-os/alchemy-lab/.claude/worktrees/site/docs/emulator
#
# writes <dest>/fw/<fw>/emu.{mjs,wasm[,data]}, <dest>/demo/{voice,beat}.wav and
# <dest>/fw/manifest.json (which source commit each build came from). The
# page itself (<dest>/index.html) lives in the site branch, not here.
set -euo pipefail
cd "$(dirname "$0")/.."
DEST=${1:?usage: web/publish.sh <site>/docs/emulator}
FWS=(belt mark smack clouds elements marbles meld plaits warps)
mkdir -p "$DEST/fw" "$DEST/demo"
python3 web/make_demos.py "$DEST/demo"
manifest="{"
# BELT_ROOT=<belt-alchemy worktree>: publish that Belt instead of fw/belt.mk's default
for f in "${FWS[@]}"; do
  if [ "$f" = belt ] && [ -n "${BELT_ROOT:-}" ]; then export FW_ROOT="$BELT_ROOT"; else unset FW_ROOT; fi
  make --no-print-directory WEB=1 FW="$f" -j8 >/dev/null
  mkdir -p "$DEST/fw/$f"
  rm -f "$DEST/fw/$f"/emu.*
  cp build/web/$f/emu.mjs build/web/$f/emu.wasm "$DEST/fw/$f/"
  [ -f build/web/$f/emu.data ] && cp build/web/$f/emu.data "$DEST/fw/$f/"
  root=$(make --no-print-directory -s WEB=1 FW="$f" print-root)
  hash=$(git -C "$root" rev-parse --short HEAD)
  branch=$(git -C "$root" rev-parse --abbrev-ref HEAD)
  manifest+="\"$f\":{\"commit\":\"$hash\",\"branch\":\"$branch\"},"
  echo "$f  $branch@$hash"
done
# Firmwares built as custom (their own repo carries emu.mk): name and FW_DIR.
# Break's worktree has empty lib/ submodules, so its SDK comes from the main
# checkout (its emu.mk says so).
TIMOS=${TIMOS:-$HOME/tim-os}
CUSTOM=(
  "stencil|$TIMOS/stencil/.claude/worktrees/firmware/alchemy"
  "break|$TIMOS/break-alchemy/.claude/worktrees/ecto-mvp"
)
for entry in "${CUSTOM[@]}"; do
  f=${entry%%|*}; dir=${entry#*|}
  extra=()
  [ "$f" = break ] && extra=(EMU_ALCHEMY_DIR="$TIMOS/break-alchemy/lib/alchemy-sdk" EMU_LIBDAISY_DIR="$TIMOS/break-alchemy/lib/libDaisy")
  make --no-print-directory WEB=1 FW=custom FW_DIR="$dir" ${extra[@]+"${extra[@]}"} -j8 >/dev/null
  out=$(make --no-print-directory -s WEB=1 FW=custom FW_DIR="$dir" ${extra[@]+"${extra[@]}"} print-build)
  mkdir -p "$DEST/fw/$f"
  rm -f "$DEST/fw/$f"/emu.*
  cp "$out"/emu.mjs "$out"/emu.wasm "$DEST/fw/$f/"
  [ -f "$out"/emu.data ] && cp "$out"/emu.data "$DEST/fw/$f/"
  hash=$(git -C "$dir" rev-parse --short HEAD)
  branch=$(git -C "$dir" rev-parse --abbrev-ref HEAD)
  manifest+="\"$f\":{\"commit\":\"$hash\",\"branch\":\"$branch\"},"
  echo "$f  $branch@$hash"
done
manifest+="\"emu\":{\"commit\":\"$(git rev-parse --short HEAD)\",\"branch\":\"$(git rev-parse --abbrev-ref HEAD)\"}}"
echo "$manifest" > "$DEST/fw/manifest.json"
du -sh "$DEST"
