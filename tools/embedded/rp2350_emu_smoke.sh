#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EMU="${RP2350_EMU_TUI:-/home/jvincent/.cargo/bin/rp2350-emu-tui}"
PICOEM="$ROOT/.radbuild/picoem"
ROM_LINK="/home/jvincent/.cargo/registry/src/roms/rp2350"
FIRMWARE="${1:-$ROOT/build/embedded/rp235x_pico/radixkernel_rp235x_smoke.bin}"
LOG="$ROOT/.radbuild/rp2350-emu-smoke.log"

if [[ ! -x "$EMU" ]]; then
    echo "rp2350-emu-tui not found; set RP2350_EMU_TUI" >&2
    exit 1
fi

if [[ ! -f "$PICOEM/roms/rp2350/bootrom-combined.bin" ]]; then
    mkdir -p "$ROOT/.radbuild"
    git clone --depth 1 https://github.com/0x4D44/picoem "$PICOEM"
fi

if [[ ! -f "$FIRMWARE" ]]; then
    echo "RP2350 firmware not found: $FIRMWARE" >&2
    echo "Build it first with: ./build.py --build-embedded --embedded-target rp235x --enable-sdio-pio --sd-mode pio_sdio" >&2
    exit 1
fi

mkdir -p "$(dirname "$ROM_LINK")" "$(dirname "$LOG")"
ln -sfn "$PICOEM/roms/rp2350" "$ROM_LINK"

set +e
if command -v script >/dev/null 2>&1; then
    TERM="${TERM:-xterm}" script -qfec "timeout 5s \"$EMU\" \"$FIRMWARE\"" "$LOG"
else
    TERM="${TERM:-xterm}" timeout 5s "$EMU" "$FIRMWARE" >"$LOG" 2>&1
fi
status=$?
set -e

if [[ "$status" != "0" && "$status" != "124" ]]; then
    cat "$LOG"
    exit "$status"
fi

grep -q "unmodelled MMIO" "$LOG"
grep -q "SHA256" "$LOG"
grep -q "COMMAND_EXIT_CODE=\"124\"" "$LOG"

echo "RP2350 emulator smoke started firmware: $LOG"
