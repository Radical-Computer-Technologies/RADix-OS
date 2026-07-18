#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export RAD_X86_UI_PROFILE=terminal
export RADLIB_X86_64_GRUB_SLINT_BUILD_DIR="${RADLIB_X86_64_GRUB_SLINT_BUILD_DIR:-$(cd "${script_dir}/../.." && pwd)/build/embedded/x86_64_grub_terminal}"
exec "${script_dir}/run_x86_64_grub_slint_vm.sh" "$@"
