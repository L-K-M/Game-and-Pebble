# CI/CD

This repository uses two GitHub Actions workflows. `build.yml` builds the Pebble app's
`.pbw` on every push and pull request and uploads it as a downloadable artifact, so you
can install Game & Pebble without setting up the SDK locally. `release.yml` runs the same
build when a version tag is pushed and attaches the `.pbw` to a GitHub Release.

## Workflows

| Workflow                                 | Trigger                                      | Purpose                                                                |
| ---------------------------------------- | -------------------------------------------- | ---------------------------------------------------------------------- |
| [`build.yml`](.github/workflows/build.yml)     | `push`, `pull_request`, `workflow_dispatch`  | Build the `.pbw` and upload it as a CI artifact (continuous build).     |
| [`release.yml`](.github/workflows/release.yml) | `push` of a tag matching `v*`                | Build the `.pbw` and attach it to a GitHub Release (tagged release).    |

## Continuous integration (`build.yml`)

Runs on `ubuntu-latest` on every push, pull request, and manual `workflow_dispatch`:

1. **Checkout** – `actions/checkout@v4`.
2. **Setup Python** – `actions/setup-python@v5`, Python 3.12 (needed for the verify script and pebble-tool).
3. **Setup Node** – `actions/setup-node@v4`, Node 20 (used by the Pebble build).
4. **Sanity-check the maze** – `python3 tools/verify_maze.py` validates the maze data before building.
5. **Install pebble-tool** – creates a venv at `$HOME/pebble-venv`, upgrades pip/setuptools/wheel,
   installs `pebble-tool`, and adds the venv's `bin` to `GITHUB_PATH`.
6. **Install Pebble SDK** – `yes | pebble sdk install latest` (auto-accepts prompts).
7. **Build** – `pebble build` compiles the app into `build/*.pbw`.
8. **Upload .pbw** – `actions/upload-artifact@v4` uploads `build/*.pbw` as the
   `Game-and-Pebble-pbw` artifact (`if-no-files-found: error`).

This gives every commit a ready-to-sideload `.pbw` without anyone needing a local Pebble
toolchain, and the maze sanity check catches bad map data early.

### Running locally

You need the Pebble SDK and `pebble-tool` (the workflow steps above mirror a local setup):

```bash
python3 tools/verify_maze.py   # sanity-check the maze data

# one-time SDK setup
python3 -m venv ~/pebble-venv
~/pebble-venv/bin/pip install --upgrade pip setuptools wheel
~/pebble-venv/bin/pip install pebble-tool
export PATH="$HOME/pebble-venv/bin:$PATH"
yes | pebble sdk install latest

pebble build                   # produces build/*.pbw
```

## Releases (`release.yml`)

Cut a release by pushing a version tag:

```
git tag v1.2.3
git push origin v1.2.3
```

The workflow runs the **same build** as `build.yml` (verify_maze → pebble-tool venv →
Pebble SDK install → `pebble build`), then:

- Copies the built `.pbw` to `build/game-and-pebble-<tag>.pbw` so the release asset name
  includes the version (e.g. `game-and-pebble-v1.2.3.pbw`).
- Creates a **GitHub Release** for the tag via `softprops/action-gh-release@v2` with
  `generate_release_notes: true` and `fail_on_unmatched_files: true` (the job fails if the
  `.pbw` is missing). Requires `permissions: contents: write` (declared at workflow level).

### Artifacts and where they are published

- **CI builds** – the `.pbw` is uploaded as the `Game-and-Pebble-pbw` artifact on each
  `build.yml` run (download it from the run's **Summary → Artifacts** section).
- **Releases** – `game-and-pebble-<tag>.pbw` is attached to the GitHub Release for the tag,
  under the repository's **Releases** page.

### Sideloading a `.pbw` onto a watch

The `.pbw` is a complete, installable Pebble app. To install it on a watch:

1. Download the `.pbw` — from a GitHub **Release** asset, or from a `build.yml` run's
   artifact (unzip the artifact to get the `.pbw`).
2. Open the **Pebble** (Rebble) mobile app, paired with your watch over Bluetooth.
3. Make the `.pbw` reachable from your phone: e.g. open the GitHub release/download link
   in the phone's browser and choose **Open in Pebble app**, AirDrop/email the file to the
   phone and open it with the Pebble app, or use a cloud/file app and "share" it to Pebble.
4. The Pebble app loads the `.pbw` and pushes it to the watch over Bluetooth; the app then
   appears in your watch's app menu.

Developers with the SDK installed can alternatively sideload directly:

```bash
pebble install --phone <PHONE_IP> build/game-and-pebble-v1.2.3.pbw   # over the local network
# or, in an emulator:
pebble install --emulator basalt build/game-and-pebble-v1.2.3.pbw
```

## Secrets

| Secret | Required? | Used by | Purpose |
| ------ | --------- | ------- | ------- |
| _none_ | —         | —       | No secrets are required. |

Both workflows run entirely on the built-in `GITHUB_TOKEN` (which
`softprops/action-gh-release@v2` uses to create the release; the workflow grants it
`contents: write`). Builds and releases work without configuring any repository secrets.
