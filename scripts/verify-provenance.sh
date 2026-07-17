#!/usr/bin/env bash
#
# Commit provenance check: fail if any commit's author, committer, or message
# carries a disallowed AI co-author attribution (identity or trailer). The match
# terms are stored base64-encoded so the tokens are not embedded verbatim in the
# tree -- a guard that spelled them out would defeat its own purpose.
#
# Usage:
#   scripts/verify-provenance.sh              # scan the entire history
#   scripts/verify-provenance.sh <git-range>  # scan a range, e.g. origin/main..HEAD
#
# Exit status is non-zero if any offending commit is found. Used by the git
# hooks in .githooks/ and the provenance CI workflow.
set -euo pipefail

# Case-insensitive alternation of disallowed provenance tokens (decoded at run
# time). If a human contributor ever legitimately matches, narrow this to the
# trailer/email forms.
PATTERN="$(printf '%s' 'Y2xhdWRlfGFudGhyb3BpYw==' | base64 -d)"

range="${1:-}"
if [ -n "$range" ]; then
    commits="$(git rev-list "$range" 2>/dev/null || true)"
else
    commits="$(git rev-list --all)"
fi

if [ -z "${commits//[[:space:]]/}" ]; then
    echo "provenance: no commits to scan."
    exit 0
fi

status=0
count=0
while IFS= read -r commit; do
    [ -n "$commit" ] || continue
    count=$((count + 1))
    meta="$(git show -s --format='%an%x1f%ae%x1f%cn%x1f%ce%x1f%B' "$commit")"
    if printf '%s' "$meta" | grep -qiE "$PATTERN"; then
        echo "provenance: FAIL -- commit carries a disallowed co-author attribution:"
        git show -s \
            --format='  commit:    %H%n  author:    %an <%ae>%n  committer: %cn <%ce>%n  subject:   %s' \
            "$commit"
        status=1
    fi
done <<< "$commits"

if [ "$status" -eq 0 ]; then
    echo "provenance: clean -- ${count} commit(s) checked."
fi
exit "$status"
