#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SLINT_REF="${SLINT_REF:-release/1.17}"
SLINT_DIR="${SLINT_SOURCE_DIR:-$ROOT/.radbuild/slint-src}"

mkdir -p "$(dirname "$SLINT_DIR")"

if [[ -d "$SLINT_DIR/.git" ]]; then
    git -C "$SLINT_DIR" fetch --depth 1 origin "$SLINT_REF"
    git -C "$SLINT_DIR" checkout --detach FETCH_HEAD
else
    rm -rf "$SLINT_DIR"
    git clone --depth 1 --branch "$SLINT_REF" https://github.com/slint-ui/slint.git "$SLINT_DIR"
fi

echo "Slint source ready: $SLINT_DIR"
