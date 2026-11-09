# PonchoWriter identity and upgrade

The completed mode/caret/code changes were fast-forwarded into the repository's default **`master`** branch at `886f608` before rebranding. There is no `main` branch. The current product repository is **https://github.com/ponchoalv/PonchoWriter**; the old URL may redirect on GitHub.

## Names and files

- Display name / QML module: **PonchoWriter**.
- Maintainer: **Alfonso Alvarez** (`ponchoalv`).
- Executable / qmake project: `ponchowriter` / `ponchowriter.pro`.
- macOS bundle: `build/ponchowriter.app`, identifier `io.github.ponchoalv.ponchowriter`, with original journal/P icon artwork.
- Linux desktop, icon and package identifiers: `ponchowriter`.
- Build/test configuration overrides now use `PONCHOWRITER_*` rather than the previous prefix. The main test executable is `tst_ponchowriter`.
- The version remains 0.7.2; identify this build by repository, branch and commit, not that unchanged version number alone.

From a fresh clone:

```sh
git clone --branch master https://github.com/ponchoalv/PonchoWriter.git ponchowriter
cd ponchowriter
./bin/build
./bin/test
# macOS:
open build/ponchowriter.app
```

Save local work before updating an existing checkout. Update `origin` deliberately if necessary; do not reset/clean notes or staged attachments. Renaming the hosted repository does not require moving an existing working directory or user data.

## Existing preferences, drafts and images

The startup compatibility component captures the exact previous Qt identity's settings and paths, then installs the new application/organization/display names. It imports only missing preference keys and remembers a profile version and storage root. New preferences always win.

**Recovery files and staged attachments are not moved or copied.** If the previous data directory exists, the new app continues using it, preserving recovery IDs, image references and per-slot locks together. A fresh installation uses the new data directory. Subsequent launches retain the chosen root. Do not delete an old-named application-data directory merely because the visible app has a new name.

The IPC directory and Windows pipe prefix deliberately remain compatible with older builds. This prevents an old and renamed binary from becoming competing primary editors. If an old copy is already running, the renamed launch forwards to it. **Quit old copies yourself, then open the new build.** This change neither terminates user processes nor replaces `/Applications` automatically.

The few old identifiers in `src/profile.h` and its tests are migration/locking interfaces, not product branding. Historical reports, Git history, protected user examples and required legal attribution are not rewritten to imply different provenance.

## License and product ownership

The root MIT license adds **Copyright (c) 2026 Alfonso Alvarez (original contributions)** and retains the original copyright, permission notice and disclaimer. See [COPYRIGHT.md](../COPYRIGHT.md). Dependencies and fonts retain their own licenses. A rename does not relicense third-party code/resources or clear the existing binary-distribution audit. Required application/font notices are now explicitly bundled with the macOS development app, alongside dependency notices.

The new icon is original PonchoWriter artwork; upstream screenshots are no longer used as product screenshots. GitHub's fork ancestry and source history are retained; the product rename is not a claim to exclusive authorship of inherited code.

## Verification

macOS arm64 / Qt 6.12.0: **352 passed, zero failures, three skipped** across twelve normal suites (129 / 16 / 23 / 13 / 9 / 42 / 6 / 12 / 48 / 9 / 5 / 40). The skipped native rendering bodies passed separately with a threaded renderer (100 mode switches); the opt-in large-note benchmark was not rerun for this identity-only change. Native spelling smoke passed without learning words. The renamed app built successfully; help/version, bundle identifier/name and bundled license/font notices were checked.

The migration regression checks missing-only import, idempotence, unchanged recovery/image bytes, retained storage root and fresh-profile fallback. A private-recovery-home subprocess smoke confirmed old-primary forwarding, renamed-primary startup and renamed-secondary forwarding. macOS native preferences are not isolated by `HOME` alone; test-created migration markers were removed afterwards, without changing the original profile or documents. Only test-owned processes were terminated. An initial smoke attempt used a too-long temporary socket path and was discarded; a short `/tmp` test home succeeded.

Windows/Linux are still source-supported but were not compiled or runtime-tested here.
