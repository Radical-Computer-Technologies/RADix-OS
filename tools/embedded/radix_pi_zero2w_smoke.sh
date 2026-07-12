#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PAYLOAD_DIR="$ROOT/tools/embedded/radix_pi_zero2w"
LOG="$ROOT/.radbuild/radix-pi-zero2w-payload-smoke.log"
QEMU="${QEMU_SYSTEM_AARCH64:-}"

if [[ -z "$QEMU" ]]; then
    if command -v qemu-system-aarch64 >/dev/null 2>&1; then
        QEMU="$(command -v qemu-system-aarch64)"
    else
        QEMU="$ROOT/.radbuild/qemu-arm/usr/bin/qemu-system-aarch64"
    fi
fi

make -C "$PAYLOAD_DIR" clean >/dev/null 2>&1 || true
make -C "$PAYLOAD_DIR" >/dev/null

if [[ ! -x "$QEMU" ]]; then
    echo "qemu-system-aarch64 not found; built payload only: $PAYLOAD_DIR/RADIXKRN.IMG"
    exit 0
fi

mkdir -p "$(dirname "$LOG")"
set +e
timeout "${RADIX_PI_QEMU_TIMEOUT:-8s}" "$QEMU" \
    -M raspi3b \
    -cpu cortex-a53 \
    -m 1G \
    -kernel "$PAYLOAD_DIR/RADIXKRN.IMG" \
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

grep -q "RADIX_PI_BCM283X_HAL_OK" "$LOG"
grep -q "RADIX_PI_UART_OK" "$LOG"
grep -q "RADIX_PI_HANDOFF_OK" "$LOG"

echo "RADix Pi Zero 2 W payload smoke passed: $LOG"
