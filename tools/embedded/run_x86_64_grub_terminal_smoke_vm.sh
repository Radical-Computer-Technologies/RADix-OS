#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

export RAD_X86_UI_PROFILE=terminal
export RAD_X86_TERMINAL_AUTOTEST_NANO=true
export RAD_X86_TERMINAL_AUTOLOGIN=true
export RAD_RKCONFIG_TERMINAL_NANO="${RAD_RKCONFIG_TERMINAL_NANO:-false}"
export RAD_RKCONFIG_TERMINAL_NANO_VARIANT="${RAD_RKCONFIG_TERMINAL_NANO_VARIANT:-none}"
export RADLIB_X86_64_GRUB_SLINT_BUILD_DIR="${RADLIB_X86_64_GRUB_SLINT_BUILD_DIR:-${repo_root}/build/embedded/x86_64_grub_terminal_smoke}"

exec "${script_dir}/run_x86_64_grub_slint_vm.sh" "$@"
