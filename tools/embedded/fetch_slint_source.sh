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

# Force the Slint Rust workspace to build the `dev` profile optimized. This is REQUIRED,
# not cosmetic: the unoptimized (opt-level=0) build produces enormous Rust stack frames, and
# a deep freestanding render overflows the kernel boot stack. Because a single >4KB frame
# steps over the lone 4KB stack guard page, the overflow silently corrupts adjacent memory
# instead of trapping -> intermittent wild #GP/#PF and Slint panics. opt-level=2 shrinks the
# frames so the render fits the stack. Idempotent: only add it if not already present.
CARGO_TOML="$SLINT_DIR/Cargo.toml"
if [[ -f "$CARGO_TOML" ]] && ! grep -qE '^\s*opt-level\s*=\s*2\b' <(awk '/^\[profile\.dev\]/{f=1;next} /^\[/{f=0} f' "$CARGO_TOML"); then
    python3 - "$CARGO_TOML" <<'PY'
import sys, re
path = sys.argv[1]
text = open(path).read()
# Insert `opt-level = 2` into the [profile.dev] section (right after its header line).
def add_opt(m):
    return m.group(0) + "opt-level = 2\n"
new = re.sub(r'(?m)^\[profile\.dev\]\n', add_opt, text, count=1)
if new == text:
    # No [profile.dev] section: append one.
    new = text.rstrip() + "\n\n[profile.dev]\nopt-level = 2\n"
open(path, "w").write(new)
PY
    echo "Patched $CARGO_TOML: [profile.dev] opt-level = 2 (kernel stack-overflow fix)"
fi

echo "Slint source ready: $SLINT_DIR"
