#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/circle_env.sh"

TARGET="pi-zero-2w-64"
CLEAN=0
CXX="--c++17"
REALTIME=0

usage() {
    cat <<'USAGE'
Usage: build_circle.sh [--target pi-zero-2w-64|pi3-64|pi4-64|pi5-64|pi2-32|pi4-32|audiohat-64] [--clean] [--c++20] [--realtime]

Configures and builds the local Circle checkout using RADLib-local toolchains.
Defaults to pi-zero-2w-64, which uses RASPPI=3 and aarch64-none-elf.
audiohat-64 builds the unique 64-bit configs needed for Pi Zero 2 W/Pi 3 and Pi 4.
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --target)
            TARGET="${2:?missing --target value}"
            shift 2
            ;;
        --clean)
            CLEAN=1
            shift
            ;;
        --c++20)
            CXX="--c++20"
            shift
            ;;
        --c++17)
            CXX="--c++17"
            shift
            ;;
        --realtime)
            REALTIME=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ ! -d "${RADLIB_CIRCLE_ROOT}" || ! -x "${RADLIB_CIRCLE_ROOT}/configure" ]]; then
    echo "Circle checkout not found at RADLIB_CIRCLE_ROOT=${RADLIB_CIRCLE_ROOT}" >&2
    exit 1
fi

build_one() {
    local target="$1"
    local rasppi
    local prefix

    case "${target}" in
        pi-zero-2w-64|pi3-64)
            rasppi=3
            prefix="${RADLIB_AARCH64_NONE_ELF_PREFIX:-}"
            ;;
        pi4-64)
            rasppi=4
            prefix="${RADLIB_AARCH64_NONE_ELF_PREFIX:-}"
            ;;
        pi5-64)
            rasppi=5
            prefix="${RADLIB_AARCH64_NONE_ELF_PREFIX:-}"
            ;;
        pi2-32)
            rasppi=2
            prefix="${RADLIB_ARM_NONE_EABI_PREFIX:-}"
            ;;
        pi4-32)
            rasppi=4
            prefix="${RADLIB_ARM_NONE_EABI_PREFIX:-}"
            ;;
        *)
            echo "unsupported target: ${target}" >&2
            exit 2
            ;;
    esac

    if [[ -z "${prefix}" || ! -x "${prefix}gcc" ]]; then
        echo "toolchain for ${target} is not installed" >&2
        echo "run: ${SCRIPT_DIR}/setup_circle_toolchains.sh --arch aarch64" >&2
        exit 1
    fi

    local configure_args=(-r "${rasppi}" -p "${prefix}" -f "${CXX}")
    if [[ "${REALTIME}" -eq 1 ]]; then
        configure_args+=(--realtime)
    fi

    echo "[circle-build] root: ${RADLIB_CIRCLE_ROOT}"
    echo "[circle-build] target: ${target}"
    echo "[circle-build] prefix: ${prefix}"

    (
        cd "${RADLIB_CIRCLE_ROOT}"
        ./configure "${configure_args[@]}"
        if [[ "${CLEAN}" -eq 1 ]]; then
            ./makeall clean
        fi
        ./makeall
    )
}

case "${TARGET}" in
    audiohat-64)
        build_one pi-zero-2w-64
        build_one pi4-64
        ;;
    *)
        build_one "${TARGET}"
        ;;
esac

echo "[circle-build] done"
