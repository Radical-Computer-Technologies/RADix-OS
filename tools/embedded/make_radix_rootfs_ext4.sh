#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
payload="${RADIX_ROOTFS_PAYLOAD:-${script_dir}/radix-rootfs-tree.tar.gz}"
output="${RADIX_ROOTFS_OUTPUT:-${script_dir}/radix-rootfs.ext4}"
size="${RADIX_ROOTFS_SIZE:-64M}"

if [[ ! -f "${payload}" ]]; then
    echo "Missing rootfs payload: ${payload}" >&2
    exit 1
fi

if ! command -v mkfs.ext4 >/dev/null 2>&1 || ! command -v debugfs >/dev/null 2>&1; then
    echo "Missing ext4 tools. Install them with: sudo apt install e2fsprogs" >&2
    exit 1
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "${tmp_dir}"' EXIT

tar -xzf "${payload}" -C "${tmp_dir}"
if [[ ! -d "${tmp_dir}/rootfs" ]]; then
    echo "Payload did not contain rootfs/" >&2
    exit 1
fi

rm -f "${output}"
truncate -s "${size}" "${output}"
mkfs.ext4 -q -F -O extent,^has_journal,^64bit,^metadata_csum "${output}"

while IFS= read -r dir; do
    rel="${dir#${tmp_dir}/rootfs}"
    [[ -z "${rel}" ]] && continue
    debugfs -w -R "mkdir ${rel}" "${output}" >/dev/null 2>&1
done < <(find "${tmp_dir}/rootfs" -type d | sort)

while IFS= read -r file; do
    rel="${file#${tmp_dir}/rootfs}"
    debugfs -w -R "write ${file} ${rel}" "${output}" >/dev/null
done < <(find "${tmp_dir}/rootfs" -type f | sort)

echo "Created ${output} (${size})"
