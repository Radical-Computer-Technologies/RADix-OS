#!/usr/bin/env bash
# Source this file to use the RADLib-local Circle toolchain without changing
# global shell startup files.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    echo "source this file: source tools/embedded/circle_env.sh" >&2
    exit 2
fi

_radlib_embedded_script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export RADLIB_ROOT="${RADLIB_ROOT:-$(cd "${_radlib_embedded_script_dir}/../.." && pwd)}"
export RADLIB_WORKSPACE_ROOT="${RADLIB_WORKSPACE_ROOT:-$(cd "${RADLIB_ROOT}/.." && pwd)}"
export RADLIB_CIRCLE_ROOT="${RADLIB_CIRCLE_ROOT:-${RADLIB_WORKSPACE_ROOT}/circle}"
export RADLIB_TOOLCHAIN_DIR="${RADLIB_TOOLCHAIN_DIR:-${RADLIB_ROOT}/.radbuild/toolchains}"

_radlib_aarch64_dir="${RADLIB_TOOLCHAIN_DIR}/arm-gnu-toolchain-15.2.rel1-x86_64-aarch64-none-elf"
_radlib_arm32_dir="${RADLIB_TOOLCHAIN_DIR}/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi"

if [[ -x "${_radlib_aarch64_dir}/bin/aarch64-none-elf-gcc" ]]; then
    export RADLIB_AARCH64_NONE_ELF_PREFIX="${_radlib_aarch64_dir}/bin/aarch64-none-elf-"
    case ":${PATH}:" in
        *":${_radlib_aarch64_dir}/bin:"*) ;;
        *) export PATH="${_radlib_aarch64_dir}/bin:${PATH}" ;;
    esac
fi

if [[ -x "${_radlib_arm32_dir}/bin/arm-none-eabi-gcc" ]]; then
    export RADLIB_ARM_NONE_EABI_PREFIX="${_radlib_arm32_dir}/bin/arm-none-eabi-"
    case ":${PATH}:" in
        *":${_radlib_arm32_dir}/bin:"*) ;;
        *) export PATH="${_radlib_arm32_dir}/bin:${PATH}" ;;
    esac
fi

unset _radlib_embedded_script_dir
unset _radlib_aarch64_dir
unset _radlib_arm32_dir
