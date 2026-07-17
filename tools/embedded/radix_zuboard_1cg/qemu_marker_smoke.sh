#!/usr/bin/env bash
# ZuBoard-1CG QEMU marker smoke: runs run-qemu.sh, captures the serial log,
# and asserts the marker set from expected-markers.txt.
#
# Markers are checked in order (each must appear at or after the previous),
# EXCEPT lines prefixed with '~', which are presence-only (checked anywhere in
# the log without advancing the ordered cursor). Use '~' for markers emitted by
# a second core: their interleaving with core-0 markers is nondeterministic, so
# a strict total order across cores is not a valid assertion.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
MARKERS_FILE="${RADIX_ZUBOARD_MARKERS:-$SCRIPT_DIR/expected-markers.txt}"
LOG_FILE="${RADIX_ZUBOARD_SERIAL_LOG:-${TMPDIR:-/tmp}/radix-zuboard-qemu-serial.log}"

# Never run forever in CI; reuse the RadBuild-built ELF and QEMU SD image.
# Default 90s: -smp 2 under single-thread TCG runs ~2x slower per core.
export RADIX_QEMU_TIMEOUT="${RADIX_QEMU_TIMEOUT:-90}"
export RADIX_QEMU_BUILD="${RADIX_QEMU_BUILD:-0}"
export RADIX_ZUBOARD_IMAGE="${RADIX_ZUBOARD_IMAGE:-$REPO_ROOT/artifacts/radix/zuboard-1cg-qemu/radix-zuboard-1cg-qemu.img}"

if [ ! -f "$MARKERS_FILE" ]; then
    echo "qemu_marker_smoke: markers file not found: $MARKERS_FILE" >&2
    exit 1
fi

mkdir -p "$(dirname "$LOG_FILE")"
set +e
"$SCRIPT_DIR/run-qemu.sh" </dev/null 2>&1 | tee "$LOG_FILE"
qemu_rc=${PIPESTATUS[0]}
set -e

# 124 = killed by timeout while idling at the serial prompt: expected.
if [ "$qemu_rc" -ne 0 ] && [ "$qemu_rc" -ne 124 ]; then
    echo "qemu_marker_smoke: unexpected qemu exit status $qemu_rc" >&2
    exit "$qemu_rc"
fi

fail=0
offset=0
while IFS= read -r marker; do
    case "$marker" in ''|'#'*) continue ;; esac
    if [ "${marker#\~}" != "$marker" ]; then
        # Presence-only (cross-core): check anywhere, do not move the cursor.
        marker="${marker#\~}"
        if grep -F -a -q -- "$marker" "$LOG_FILE"; then
            echo "ok (present): $marker"
        else
            echo "MISSING: $marker" >&2
            fail=1
        fi
        continue
    fi
    hit="$(tail -c +$((offset + 1)) "$LOG_FILE" \
        | grep -F -a -b -m1 -o -- "$marker" | head -n1 | cut -d: -f1 || true)"
    if [ -z "$hit" ]; then
        echo "MISSING/OUT-OF-ORDER: $marker" >&2
        fail=1
    else
        offset=$((offset + hit + ${#marker}))
        echo "ok: $marker"
    fi
done < "$MARKERS_FILE"

if [ "$fail" -ne 0 ]; then
    echo "qemu_marker_smoke: marker gate FAILED (serial log: $LOG_FILE)" >&2
    exit 1
fi
echo "qemu_marker_smoke: all markers present in order"
