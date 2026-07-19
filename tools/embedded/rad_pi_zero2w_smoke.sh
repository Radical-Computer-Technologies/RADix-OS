#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PAYLOAD_DIR="$ROOT/tools/embedded/rad_pi_zero2w"
LOG="$ROOT/.radbuild/rad-pi-zero2w-payload-smoke.log"

# shellcheck source=/dev/null
source "$ROOT/tools/embedded/qemu_aarch64_env.sh"
QEMU="$(rad_resolve_qemu_system_aarch64 "$ROOT")"

make -C "$PAYLOAD_DIR" clean >/dev/null 2>&1 || true
make -C "$PAYLOAD_DIR" >/dev/null

mkdir -p "$(dirname "$LOG")"
set +e
timeout "${RAD_PI_QEMU_TIMEOUT:-8s}" "$QEMU" \
    -M raspi3b \
    -cpu cortex-a53 \
    -m 1G \
    -kernel "$PAYLOAD_DIR/RADKRN.IMG" \
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

require_marker() {
    local marker="${1:?missing marker}"
    if ! grep -q "$marker" "$LOG"; then
        cat "$LOG"
        echo "RADPx-OS Pi Zero 2 W payload smoke missing marker: $marker" >&2
        exit 1
    fi
}

# Gate against the ordered parity marker set (kept in expected-markers.txt next to
# the board sources). This is the set the bcm283x Pi Zero 2 W boots reliably from a
# kernel-only image under QEMU raspi3b: the SoC HAL bring-up, the shared A53
# kernel (MMU/exception/EL0/process-arch), the x86<->a53 parity self-tests
# (kernel-infra, in-guest UDP+TCP L4, named services, base-terminal), preemption
# (QA7 local-timer + EL1 CNTP: RAD_TIMER_IRQ_OK / RAD_PREEMPT_SCHED_OK), the full
# preemptive userland smoke (init -> radsh -> fork/COW/exec/wait/reap/shebang:
# RAD_AARCH64_FORK/USER_FORK/COW_PAGE_FAULT/USER_ZOMBIE_REAP/USER_EXECVE/
# USER_EXECVE_REENTER/USER_PIPE_FORK/USER_WAIT_WAKE/USER_SCRIPT_SHEBANG/
# USERMODE_EXIT_OK), and the framebuffer/compositor path.
GATE="$ROOT/tools/embedded/rad_pi_zero2w/expected-markers.txt"
while IFS= read -r marker; do
    [[ -z "$marker" || "$marker" == \#* ]] && continue
    require_marker "$marker"
done < "$GATE"

# DEFERRED -- the remaining ZuBoard markers not covered here need infrastructure
# the Pi payload does not carry (it is a kernel-only image, no rootfs / no NIC):
#   RAD_SERVICE_ROOTFS_OK / RAD_ZUBOARD_EXT4_ROOT_OK -- need an ext4 SD rootfs
#     image (the Pi boots kernel-only with an embedded /bin; wiring a radbuild Pi
#     SD image is the follow-up).
#   RAD_LOGIN_OK -- the embedded init execs radsh directly; the full login.elf
#     flow needs the rootfs userland.
#   RAD_NET_HOST_UDP_ECHO_OK / RAD_NTP_* -- need a hardware NIC + SLIRP host
#     responder; raspi3b has no GEM-equivalent (in-guest UDP/TCP L4 loopback is
#     covered above and is NIC-independent).

echo "RADPx-OS Pi Zero 2 W payload smoke passed ($(grep -cvE '^\s*(#|$)' "$GATE") markers): $LOG"
