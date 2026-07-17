# Git hooks — commit provenance

These hooks keep any commit carrying a disallowed AI co-author attribution
(author, committer, or message trailer) out of the repository. The match terms
live base64-encoded in `scripts/verify-provenance.sh` so the guard does not
spell them out in the tree.

## Enable (once per clone)

```bash
git config core.hooksPath .githooks
```

That points git at this directory instead of `.git/hooks`. The hooks are tracked
in the repo, so every clone gets them once this is run.

## What runs

- **`pre-commit`** — rejects the commit if the author/committer identity that git
  would stamp matches a disallowed token.
- **`commit-msg`** — rejects the commit if the message body matches (catches
  co-author trailers and bot addresses).

Both share the pattern used by `scripts/verify-provenance.sh`, which the
`provenance` GitHub Actions workflow runs server-side over every pushed range —
so a commit made with the hooks disabled is still caught at CI.

## Verify history

```bash
scripts/verify-provenance.sh            # scans the entire history
```
