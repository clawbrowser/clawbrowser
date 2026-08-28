# Auth On Missing API Key Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Route Clawbrowser to the auth page whenever no API key can be resolved from environment or config, even if a cached fingerprint profile exists.

**Architecture:** Keep API key resolution centralized in `ProfileManager::ResolveApiKey()` and tighten startup gating around that resolved value. Early startup and main startup should both treat a missing resolved API key as an auth prerequisite for fingerprint mode so normal launches and automation share the same rule.

**Tech Stack:** C++, gtest, Clawbrowser startup/auth flow

---

### Task 1: Lock the missing-key behavior with tests

**Files:**
- Modify: `clawbrowser/test/startup_unittest.cc`
- Test: `clawbrowser/test/startup_unittest.cc`

**Step 1: Write the failing tests**

- Update the cached-profile startup expectation to open `clawbrowser://auth/` when no API key is available.
- Add early-startup coverage showing an implicit cached profile still routes to auth without a resolved API key.

**Step 2: Run test to verify it fails**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter='StartupTest.ConfigureEarlyStartupUsesCachedProfileBeforeDefault:StartupTest.FingerprintCachedProfileWithoutApiKey'`

Expected: FAIL because startup currently allows cached profiles to launch without an API key.

### Task 2: Tighten startup gating

**Files:**
- Modify: `clawbrowser/startup.cc`
- Test: `clawbrowser/test/startup_unittest.cc`

**Step 1: Write minimal implementation**

- Change fingerprint-mode startup gating so `!ResolveApiKey().has_value()` routes to auth regardless of cache presence.
- Leave vanilla-mode auth behavior unchanged.

**Step 2: Run test to verify it passes**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter='StartupTest.ConfigureEarlyStartupUsesCachedProfileBeforeDefault:StartupTest.FingerprintCachedProfileWithoutApiKey'`

Expected: PASS

### Task 3: Verify the focused startup suite

**Files:**
- Test: `clawbrowser/test/startup_unittest.cc`

**Step 1: Run broader startup coverage**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter='StartupTest.*'`

Expected: PASS
