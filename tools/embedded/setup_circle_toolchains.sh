#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RADLIB_ROOT="${RADLIB_ROOT:-$(cd "${SCRIPT_DIR}/../.." && pwd)}"
TOOLCHAIN_DIR="${RADLIB_TOOLCHAIN_DIR:-${RADLIB_ROOT}/.radbuild/toolchains}"
DOWNLOAD_DIR="${RADLIB_DOWNLOAD_DIR:-${RADLIB_ROOT}/.radbuild/downloads}"
ARCH="aarch64"

usage() {
    cat <<'USAGE'
Usage: setup_circle_toolchains.sh [--arch aarch64|arm32|both] [--toolchain-dir DIR]

Downloads and extracts Arm GNU Toolchain 15.2.Rel1 into a local RADLib cache.
No files are installed under /usr and no shell startup files are modified.
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch)
            ARCH="${2:?missing --arch value}"
            shift 2
            ;;
        --toolchain-dir)
            TOOLCHAIN_DIR="${2:?missing --toolchain-dir value}"
            shift 2
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

case "${ARCH}" in
    aarch64|arm32|both) ;;
    *)
        echo "unsupported --arch '${ARCH}'" >&2
        exit 2
        ;;
esac

require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required host tool: $1" >&2
        exit 1
    fi
}

download_and_extract() {
    local triple="$1"
    local archive="arm-gnu-toolchain-15.2.rel1-x86_64-${triple}.tar.xz"
    local target="${TOOLCHAIN_DIR}/arm-gnu-toolchain-15.2.rel1-x86_64-${triple}"
    local url="https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/${archive}"
    local asc_url="${url}.asc"
    local archive_path="${DOWNLOAD_DIR}/${archive}"
    local asc_path="${archive_path}.asc"

    if [[ -x "${target}/bin/${triple}-gcc" ]]; then
        echo "[circle-toolchain] ${triple} already installed at ${target}"
        return 0
    fi

    mkdir -p "${TOOLCHAIN_DIR}" "${DOWNLOAD_DIR}"
    if [[ ! -s "${archive_path}" ]]; then
        echo "[circle-toolchain] downloading ${archive}"
        curl -L --fail --retry 3 --output "${archive_path}.tmp" "${url}"
        mv "${archive_path}.tmp" "${archive_path}"
    fi

    if [[ ! -s "${asc_path}" ]]; then
        curl -L --fail --retry 3 --output "${asc_path}.tmp" "${asc_url}" || rm -f "${asc_path}.tmp"
        [[ -f "${asc_path}.tmp" ]] && mv "${asc_path}.tmp" "${asc_path}"
    fi

    if [[ -s "${asc_path}" ]]; then
        local expected
        expected="$(awk '{print $1; exit}' "${asc_path}")"
        if [[ -n "${expected}" ]]; then
            echo "${expected}  ${archive_path}" | md5sum -c -
        fi
    fi

    echo "[circle-toolchain] extracting ${archive}"
    tar -C "${TOOLCHAIN_DIR}" -xf "${archive_path}"
    "${target}/bin/${triple}-gcc" --version | sed -n '1p'
}

require_tool curl
require_tool tar
require_tool xz
require_tool md5sum

if [[ "${ARCH}" == "aarch64" || "${ARCH}" == "both" ]]; then
    download_and_extract "aarch64-none-elf"
fi

if [[ "${ARCH}" == "arm32" || "${ARCH}" == "both" ]]; then
    download_and_extract "arm-none-eabi"
fi

echo "[circle-toolchain] done"
echo "source ${RADLIB_ROOT}/tools/embedded/circle_env.sh"
