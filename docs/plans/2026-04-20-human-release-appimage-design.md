# Human Release AppImage Design

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a separate Linux AppImage release path that reuses the existing incremental Linux build outputs and ships `clawbrowser-human-release-x64.AppImage` plus `clawbrowser-human-release-arm64.AppImage`.

**Architecture:** Keep the current Linux `tar.gz` prod artifacts unchanged as the build/debug release. Add an AppImage repackaging step that consumes the already staged runtime payload, adds the minimal AppImage metadata (`AppRun`, `.desktop`, icon), and generates one AppImage per architecture. Install and cache `appimagetool` on Linux builders through the remote setup script so any Linux host can prepare for the release path without manual setup.

**Tech Stack:** Bash, rsync, AppImage tooling (`appimagetool`), shell contract tests

---

### Task 1: Bootstrap AppImage tooling on Linux builders

**Files:**
- Modify: `scripts/clawbrowser_remote.sh`
- Test: `scripts/clawbrowser_remote_test.sh`

**Step 1: Write the failing test**

- Add contract coverage that Linux setup checks for the AppImage tool and installs or downloads it when missing.

**Step 2: Run test to verify it fails**

Run: `bash scripts/clawbrowser_remote_test.sh`
Expected: FAIL because the AppImage tool path is not yet asserted.

**Step 3: Write minimal implementation**

- Add an `ensure_appimagetool` helper for Linux.
- Make `machine-setup` and Linux patch/apply flows invoke it.

**Step 4: Run test to verify it passes**

Run: `bash scripts/clawbrowser_remote_test.sh`
Expected: PASS

### Task 2: Add AppImage repackaging to the release helper

**Files:**
- Modify: `scripts/build_remote_prod_artifacts.sh`
- Modify: `scripts/build_remote_prod_artifacts_test.sh`

**Step 1: Write the failing test**

- Add Linux contract assertions for the AppImage staging helpers, metadata files, and `clawbrowser-human-release-*.AppImage` outputs.

**Step 2: Run test to verify it fails**

Run: `bash scripts/build_remote_prod_artifacts_test.sh`
Expected: FAIL because the AppImage path does not exist yet.

**Step 3: Write minimal implementation**

- Stage an AppDir from the existing Linux runtime payload.
- Add `AppRun`, a desktop file, and a top-level icon.
- Invoke `appimagetool` for x64 and arm64 using the same incremental build outputs.
- Keep the existing tar.gz artifacts unchanged.

**Step 4: Run test to verify it passes**

Run: `bash scripts/build_remote_prod_artifacts_test.sh`
Expected: PASS

### Task 3: Rebuild and verify the release artifacts

**Files:**
- Test: generated Linux release artifacts on the remote builder

**Step 1: Run the remote artifact flow**

Run: `bash scripts/build_remote_prod_artifacts.sh --appimage-release-name human-release`

Expected: successful tar.gz artifacts plus `clawbrowser-human-release-x64.AppImage` and `clawbrowser-human-release-arm64.AppImage`

**Step 2: Validate the AppImages**

Run remote checks:
- `file clawbrowser-human-release-*.AppImage`
- `chmod +x clawbrowser-human-release-*.AppImage && ./clawbrowser-human-release-x64.AppImage --version`

Expected: both artifacts are executable AppImages and launch the browser wrapper correctly
