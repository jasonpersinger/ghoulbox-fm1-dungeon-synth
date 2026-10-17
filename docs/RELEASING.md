# Releasing GHOULBOX

A release is a GitHub release `ghoulbox-vX.Y` carrying `ghoulbox-X.Y.fwsc` (the FM-1 Firmware Hub looks for that
name), plus the site at https://ghoulbox.messy.quest updated to offer it. `tools/release.py` does every step
that needs no key; the steps that publish need the owner's GPG key and are done by hand.

## 1. Before

- [ ] The changes are merged into `ghoulbox` through a pull request, CI green.
- [ ] `GHOULBOX_VERSION` in `firmware/src/core.h` is the new version (`FELUCCA_VERSION` is upstream's; leave it).
- [ ] User-facing changes are in `ROADMAP.md` / the guide where they belong.
- [ ] Release notes written, in the format of the last release (install steps with the backup first, links, what's
      new, "Your saved music", notes).

## 2. Prepare

From a clean checkout of `origin/ghoulbox` (the `dev` worktree, fast-forwarded):

```sh
tools/release.py X.Y
```

It stops at the first failure. It checks git and the version, builds (the slot spare gate included), runs every
test, builds and checks the emulator, builds the site with the sound page, and leaves in `build/release-gbX.Y/`:
the package, `SHA256SUMS` and `site/`. It ends by printing the publish commands with the commit filled in.

`tools/release.py X.Y --dry-run` runs the same on any commit (no git checks): to rebuild an old release, or to try
a branch before it is merged.

## 3. On the FM-1

Install `build/release-gbX.Y/ghoulbox-X.Y.fwsc` (`build/venv/bin/python tools/fm1_install.py FILE`, or the
installer page served locally), after a backup in the web editor. Then:

- [ ] MENU > SYSTEM > ABOUT shows X.Y.
- [ ] The power-on scene plays; every track sounds; HALL and TAPE on.
- [ ] One sound of each engine that changed, against the emulator (the same, by ear).
- [ ] Save a project, power off and on, load it.
- [ ] The web editor connects and reads the version.
- [ ] Whatever the release changed, tried by hand.

## 4. Publish (the owner's key)

In this order, the commands as `tools/release.py` printed them:

1. Signed tag on the merged commit, pushed: `git tag -s ghoulbox-vX.Y COMMIT -m "..."`.
2. `gh release create ghoulbox-vX.Y build/release-gbX.Y/ghoulbox-X.Y.fwsc --title "GHOULBOX X.Y" --notes-file NOTES.md --latest`
   (and the guide PDF, when it changed).
3. The site: a worktree of `origin/gh-pages`, `rsync` the built site over it keeping `.git`, `.github` and
   `.nojekyll`, check `git status` (only the package, the emulator, the sound clips and version text should
   change), a signed commit, a normal push (never force: `gh-pages` keeps Codex's and others' commits).

## 5. After

- [ ] Download the release asset and compare its sha256 with `SHA256SUMS`.
- [ ] Wait for the Pages build (`gh api repos/jasonpersinger/ghoulbox-fm1-dungeon-synth/pages/builds/latest`), then
      the live installer offers `gbX.Y` and the emulator shows `GHOULBOX X.Y`.
- [ ] Remove the `build/ghpages` worktree and its branch.
