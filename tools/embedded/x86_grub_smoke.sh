#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build/embedded/x86_grub}"
LOG="$ROOT/.radbuild/x86-grub-smoke.log"

if ! command -v mformat >/dev/null 2>&1; then
    echo "x86 GRUB smoke requires mtools (mformat) for grub-mkrescue ISO packaging" >&2
    exit 1
fi

cmake -S "$ROOT/tools/embedded/x86_grub" -B "$BUILD_DIR" >/dev/null
cmake --build "$BUILD_DIR" --target radkernel_x86_grub_iso -j2 >/dev/null

mkdir -p "$(dirname "$LOG")"
timeout 12s qemu-system-x86_64 \
    -cdrom "$BUILD_DIR/radkernel-x86-grub.iso" \
    -serial stdio \
    -display none \
    -no-reboot \
    >"$LOG" 2>&1 || status=$?
status="${status:-0}"
if [[ "$status" != "0" && "$status" != "124" ]]; then
    cat "$LOG"
    exit "$status"
fi

grep -q "RAD x86 GRUB handoff" "$LOG"
grep -q "RAD x86 GRUB smoke ready" "$LOG"
echo "x86 GRUB smoke passed: $LOG"
