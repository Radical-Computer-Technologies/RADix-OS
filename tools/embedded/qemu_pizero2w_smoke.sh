#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CIRCLEHOME="${CIRCLEHOME:-/media/jvincent/Kingspec512/repos/circle}"
TOOLCHAIN="$ROOT/.radbuild/toolchains/arm-gnu-toolchain-15.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-"
QEMU="${QEMU_SYSTEM_AARCH64:-}"
LOG="$ROOT/.radbuild/qemu-pizero2w-smoke.log"

if [[ -z "$QEMU" ]]; then
    if command -v qemu-system-aarch64 >/dev/null 2>&1; then
        QEMU="$(command -v qemu-system-aarch64)"
    else
        QEMU="$ROOT/.radbuild/qemu-arm/usr/bin/qemu-system-aarch64"
    fi
fi

if [[ ! -x "$QEMU" ]]; then
    mkdir -p "$ROOT/.radbuild/qemu-arm-deb" "$ROOT/.radbuild/qemu-arm"
    (
        cd "$ROOT/.radbuild/qemu-arm-deb"
        apt-get download qemu-system-arm
        dpkg-deb -x qemu-system-arm_*.deb "$ROOT/.radbuild/qemu-arm"
    )
fi

if [[ ! -x "$QEMU" ]]; then
    echo "qemu-system-aarch64 not found; install qemu-system-arm or set QEMU_SYSTEM_AARCH64" >&2
    exit 1
fi

cd "$CIRCLEHOME"
./configure -f -r 3 -p "$TOOLCHAIN" --multicore --qemu --c++20 >/dev/null
./makeall >/dev/null

cd "$ROOT"
make -C tools/embedded/circle_pizero2w clean >/dev/null 2>&1 || true
make -C tools/embedded/circle_pizero2w QEMU=1 >/dev/null

mkdir -p "$(dirname "$LOG")"
set +e
timeout 12s "$QEMU" \
    -M raspi3b \
    -cpu cortex-a53 \
    -m 1G \
    -kernel "$ROOT/tools/embedded/circle_pizero2w/kernel8.img" \
    -serial stdio \
    -display none \
    -no-reboot \
    >"$LOG" 2>&1
status=$?
set -e

if [[ "$status" != "0" && "$status" != "124" ]]; then
    cat "$LOG"
    exit "$status"
fi

grep -q "backend=circle_pi" "$LOG"
grep -q "detected=4 service_core=0 worker_cores=3" "$LOG"
grep -q "dsp_samples=96" "$LOG"

if [[ "${RADLIB_RESTORE_CIRCLE_HW:-1}" == "1" ]]; then
    (
        cd "$CIRCLEHOME"
        ./configure -f -r 3 -p "$TOOLCHAIN" --multicore --c++20 >/dev/null
        ./makeall >/dev/null
    )
fi

echo "Pi Zero 2 W QEMU smoke passed: $LOG"
