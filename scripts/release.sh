#!/usr/bin/env bash
# Cuts a release: bumps the version, commits, tags "v<version>", and with --push
# pushes branch + tag — which triggers .github/workflows/release.yml to build the
# .pbw and attach it to a GitHub Release. IMPORTANT: CI builds the .pbw from the
# *committed* version (`pebble build` ships package.json's top-level "version" as
# the watch-visible versionLabel) and only *names* the release and its asset from
# the tag (game-and-pebble-v1.1.0.pbw) — it does NOT derive the app version from
# the tag, so the committed version and the tag must agree; this script keeps them
# in step. Versions are X.Y (the SDK docs' form) or X.Y.Z; pebble-tool accepts both.
#
#   scripts/release.sh 1.1.0          # bump package.json + README, commit, tag v1.1.0
#   scripts/release.sh 1.1.0 --push   # …also push the commit + tag (CI then publishes)
#   scripts/release.sh                # tag the current version as-is
#
# Usage: scripts/release.sh [X.Y[.Z]] [--push]
# Shared engine: https://github.com/L-K-M/release-tool (this stub only sets config).
set -euo pipefail

export RELEASE_APP_NAME="Game & Pebble"
export RELEASE_KIND="pebble"
export RELEASE_CI_NOTE="CI (release.yml) will now build the .pbw and publish the GitHub Release for the tag."
export RELEASE_INVOKED_AS="scripts/release.sh"

BIN="${LKM_RELEASE_BIN:-lkm-release}"
command -v "$BIN" >/dev/null 2>&1 || {
  echo "error: lkm-release not found — clone https://github.com/L-K-M/release-tool and run ./install.sh" >&2
  exit 1
}
exec "$BIN" "$@"
