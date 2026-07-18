#!/usr/bin/env bash
# ZuBoard-1CG SMP smoke: boots under -smp 2 and presence-checks the
# second-core (AP) markers. Kept separate from qemu_marker_smoke.sh because
# under single-thread TCG two cores interleave nondeterministically and boot
# runs ~2x slower, so the ordered core-0 gate and interactive login run -smp 1.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
LOG_FILE="${RAD_ZUBOARD_SMP_LOG:-${TMPDIR:-/tmp}/rad-zuboard-smp-serial.log}"

export RAD_QEMU_SMP="${RAD_QEMU_SMP:-2}"
export RAD_QEMU_TIMEOUT="${RAD_QEMU_TIMEOUT:-90}"
export RAD_QEMU_BUILD="${RAD_QEMU_BUILD:-0}"
export RAD_ZUBOARD_IMAGE="${RAD_ZUBOARD_IMAGE:-$REPO_ROOT/artifacts/rad/zuboard-1cg-qemu/rad-zuboard-1cg-qemu.img}"

# Core-1 proof: booted, own timer PPI, worker mask set, a task dispatched there
# and run to completion. NOT gated:
# - RAD_AP_PREEMPT_SCHED_OK: preemption fires only when the timer interrupts
#   EL0 (x86 ring-3 parity; ticking kernel code corrupts in-flight dispatches),
#   and user tasks are pinned to core 0 this pass -- so AP preemption is
#   unreachable until users run on APs.
AP_MARKERS=(
    RAD_AP_START_OK
    RAD_AP_TIMER_IRQ_OK
    RAD_AP_WORKERS_ONLINE_OK
    RAD_AP_WORKER_TASK_OK
    RAD_AP_SCHED_STRESS_OK
)

mkdir -p "$(dirname "$LOG_FILE")"

run_and_check() {
    set +e
    "$SCRIPT_DIR/run-qemu.sh" </dev/null 2>&1 | tee "$LOG_FILE"
    local qemu_rc=${PIPESTATUS[0]}
    set -e
    if [ "$qemu_rc" -ne 0 ] && [ "$qemu_rc" -ne 124 ]; then
        echo "qemu_smp_smoke: unexpected qemu exit status $qemu_rc" >&2
        return 2
    fi
    local fail=0
    for marker in "${AP_MARKERS[@]}"; do
        if grep -F -a -q -- "$marker" "$LOG_FILE"; then
            echo "ok: $marker"
        else
            echo "MISSING: $marker" >&2
            fail=1
        fi
    done
    # No RAD_AARCH64_*_EXCEPTION (a fault on either core).
    if grep -F -a -q -- "RAD_AARCH64_KERNEL_EXCEPTION" "$LOG_FILE" \
        || grep -F -a -q -- "RAD_AARCH64_USER_EXCEPTION" "$LOG_FILE"; then
        echo "MISSING: unexpected CPU exception in SMP boot" >&2
        fail=1
    fi
    return "$fail"
}

# One retry: under single-thread TCG without -icount the guest counter follows
# host wall-clock while guest progress depends on host CPU share, so a loaded
# host intermittently (observed ~1 in 5 under parallel load) starves the boot
# past its in-guest windows. A genuine SMP regression fails both attempts.
if run_and_check; then
    echo "qemu_smp_smoke: all AP markers present"
    exit 0
fi
echo "qemu_smp_smoke: first attempt failed; retrying once (host-load timing artifact)" >&2
if run_and_check; then
    echo "qemu_smp_smoke: all AP markers present (second attempt)"
    exit 0
fi
echo "qemu_smp_smoke: FAILED on both attempts (serial log: $LOG_FILE)" >&2
exit 1
