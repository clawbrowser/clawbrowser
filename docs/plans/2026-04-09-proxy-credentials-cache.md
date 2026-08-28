# Proxy Credential Cache Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Persist proxy credentials once per fingerprint profile as encrypted data, reuse them from the local profile cache, and refresh them only when a profile is regenerated or force-refreshed.

**Architecture:** Keep `Browser/<fp_id>/fingerprint.json` as the single profile cache, add an AES-GCM encrypted credential blob to the profile envelope, and hydrate runtime proxy state by decrypting that blob when cached profiles are loaded. Startup keeps its existing fetch decision logic, but cache writes strip plaintext proxy secrets before persisting and cache reads restore decrypted secrets for proxy auth and verify flows.

**Tech Stack:** Chromium C++, `base::JSONReader`/`base::JSONWriter`, `crypto::Aead`, gtest, existing `ProfileManager`/`ProfileEnvelope`/startup code.

---

### Task 1: Add failing envelope tests for encrypted proxy credentials

**Files:**
- Modify: `clawbrowser/test/profile_envelope_unittest.cc`
- Test: `clawbrowser/test/profile_envelope_unittest.cc`

**Step 1: Write the failing test**

Add tests that assert:

- schema version 2 profiles can parse `encrypted_proxy_credentials`
- serializing a `ProfileEnvelope` with proxy creds does not emit plaintext `username` or `password`
- older schema version 1 fixtures with plaintext proxy creds still parse

Example test shape:

```cpp
TEST(ProfileEnvelopeTest, SerializeEncryptsProxyCredentials) {
  ProfileEnvelope envelope;
  envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
  envelope.created_at = "2026-04-09T12:00:00Z";
  envelope.request.platform = "macos";
  envelope.request.browser = "clawbrowser";
  envelope.request.country = "US";
  envelope.response = MakeResponseWithProxy("user_abc", "pass_xyz");

  std::string json = envelope.Serialize();

  EXPECT_EQ(json.find("user_abc"), std::string::npos);
  EXPECT_EQ(json.find("pass_xyz"), std::string::npos);
  EXPECT_NE(json.find("encrypted_proxy_credentials"), std::string::npos);
}
```

**Step 2: Run test to verify it fails**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter=ProfileEnvelopeTest.SerializeEncryptsProxyCredentials:ProfileEnvelopeTest.ParseValidEnvelope`

Expected: FAIL because no encrypted field exists yet and plaintext creds still round-trip.

**Step 3: Write minimal implementation**

Do not implement full startup changes yet. Only add the envelope-level code needed to express encrypted proxy credentials in serialization and parsing.

**Step 4: Run test to verify it passes**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter=ProfileEnvelopeTest.SerializeEncryptsProxyCredentials:ProfileEnvelopeTest.ParseValidEnvelope`

Expected: PASS

**Step 5: Commit**

```bash
git add clawbrowser/test/profile_envelope_unittest.cc clawbrowser/profile_envelope.h clawbrowser/profile_envelope.cc
git commit -m "test: cover encrypted proxy credentials in profile envelope"
```

### Task 2: Add crypto helper for proxy credential encryption and decryption

**Files:**
- Create: `clawbrowser/proxy/proxy_credentials_crypto.h`
- Create: `clawbrowser/proxy/proxy_credentials_crypto.cc`
- Modify: `clawbrowser/BUILD.gn`
- Test: `clawbrowser/test/proxy_credentials_crypto_unittest.cc`

**Step 1: Write the failing test**

Add tests that assert:

- encrypt/decrypt round-trip returns the original username/password
- encryption output changes across calls because the nonce is random
- decryption fails for malformed blobs

Example:

```cpp
TEST(ProxyCredentialsCryptoTest, RoundTripsCredentials) {
  auto encrypted = EncryptProxyCredentials("user_abc", "pass_xyz");
  ASSERT_TRUE(encrypted.has_value()) << encrypted.error();

  auto decrypted = DecryptProxyCredentials(*encrypted);
  ASSERT_TRUE(decrypted.has_value()) << decrypted.error();
  EXPECT_EQ(decrypted->username, "user_abc");
  EXPECT_EQ(decrypted->password, "pass_xyz");
}
```

**Step 2: Run test to verify it fails**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter=ProxyCredentialsCryptoTest.*`

Expected: FAIL because the helper does not exist yet.

**Step 3: Write minimal implementation**

Implement a small helper around `crypto::Aead` with:

- a hardcoded 32-byte AES-256 key
- random nonce generation
- a compact JSON or delimiter-encoded blob format
- explicit parse/validation errors

**Step 4: Run test to verify it passes**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter=ProxyCredentialsCryptoTest.*`

Expected: PASS

**Step 5: Commit**

```bash
git add clawbrowser/proxy/proxy_credentials_crypto.h clawbrowser/proxy/proxy_credentials_crypto.cc clawbrowser/test/proxy_credentials_crypto_unittest.cc clawbrowser/BUILD.gn
git commit -m "feat: add proxy credential encryption helper"
```

### Task 3: Persist encrypted proxy credentials in the profile envelope

**Files:**
- Modify: `clawbrowser/profile_envelope.h`
- Modify: `clawbrowser/profile_envelope.cc`
- Modify: `clawbrowser/cli/profile_manager.cc`
- Test: `clawbrowser/test/profile_envelope_unittest.cc`
- Test: `clawbrowser/test/profile_manager_unittest.cc`

**Step 1: Write the failing test**

Add tests that assert:

- `ProfileManager::SaveProfile()` writes encrypted creds and does not write plaintext creds
- `ProfileManager::ReadProfile()` restores proxy creds into the returned envelope

Example:

```cpp
TEST_F(ProfileManagerTest, SaveAndReadEnvelopeRestoresEncryptedProxyCredentials) {
  ProfileEnvelope envelope = MakeEnvelopeWithProxy("user_abc", "pass_xyz");

  ASSERT_TRUE(manager_->SaveProfile("fp_test", envelope).has_value());

  std::string raw_json;
  ASSERT_TRUE(base::ReadFileToString(manager_->GetFingerprintPath("fp_test"), &raw_json));
  EXPECT_EQ(raw_json.find("user_abc"), std::string::npos);
  EXPECT_EQ(raw_json.find("pass_xyz"), std::string::npos);

  auto read_result = manager_->ReadProfile("fp_test");
  ASSERT_TRUE(read_result.has_value()) << read_result.error();
  ASSERT_TRUE(read_result->response.proxy.has_value());
  EXPECT_EQ(read_result->response.proxy->username, "user_abc");
  EXPECT_EQ(read_result->response.proxy->password, "pass_xyz");
}
```

**Step 2: Run test to verify it fails**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter=ProfileManagerTest.SaveAndReadEnvelopeRestoresEncryptedProxyCredentials`

Expected: FAIL because the persisted JSON still contains plaintext creds or read-back does not decrypt them.

**Step 3: Write minimal implementation**

Update envelope serialization/parsing so:

- schema version is bumped
- plaintext proxy creds are stripped before writing
- encrypted blob is written separately
- parse restores decrypted creds into `response.proxy`
- old schema version 1 files with plaintext creds still load

**Step 4: Run test to verify it passes**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter=ProfileManagerTest.SaveAndReadEnvelopeRestoresEncryptedProxyCredentials`

Expected: PASS

**Step 5: Commit**

```bash
git add clawbrowser/profile_envelope.h clawbrowser/profile_envelope.cc clawbrowser/cli/profile_manager.cc clawbrowser/test/profile_envelope_unittest.cc clawbrowser/test/profile_manager_unittest.cc
git commit -m "feat: encrypt cached proxy credentials"
```

### Task 4: Restore decrypted credentials in runtime load paths

**Files:**
- Modify: `clawbrowser/fingerprint_loader.cc`
- Test: `clawbrowser/test/fingerprint_loader_unittest.cc`

**Step 1: Write the failing test**

Add a test that loads a cached profile containing encrypted creds and verifies:

- `FingerprintAccessor::GetProxy()` is populated
- username/password are restored in runtime state
- child payload generation carries decrypted creds into the child path

**Step 2: Run test to verify it fails**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter=FingerprintLoaderTest.ChildPayloadPreservesProxyConfig`

Expected: FAIL because runtime load/build paths do not yet work with the encrypted cache format.

**Step 3: Write minimal implementation**

Adjust the loader only as needed so cached encrypted profiles and child payload generation still expose decrypted credentials to runtime consumers.

**Step 4: Run test to verify it passes**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter=FingerprintLoaderTest.ChildPayloadPreservesProxyConfig`

Expected: PASS

**Step 5: Commit**

```bash
git add clawbrowser/fingerprint_loader.cc clawbrowser/test/fingerprint_loader_unittest.cc
git commit -m "fix: restore proxy credentials from cached profiles"
```

### Task 5: Cover startup refresh and overwrite behavior

**Files:**
- Modify: `clawbrowser/startup.cc`
- Test: `clawbrowser/test/startup_unittest.cc`

**Step 1: Write the failing test**

Add tests that assert:

- cached profiles do not trigger a refetch when reuse is allowed
- `--regenerate` overwrites old encrypted credential state with fresh backend values
- decrypt failures surface as load errors or trigger a refresh path when appropriate

Example:

```cpp
TEST_F(StartupTest, RegenerateOverwritesEncryptedProxyCredentials) {
  WriteCachedProfileWithProxy("fp_regen", "stale_user", "stale_pass");
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  AddGenerateResponseWithProxy("fresh_user", "fresh_pass");

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "fp_regen");
  cmd.AppendSwitch("regenerate");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);

  ProfileEnvelope saved = ReadSavedProfile("fp_regen");
  ASSERT_TRUE(saved.response.proxy.has_value());
  EXPECT_EQ(saved.response.proxy->username, "fresh_user");
  EXPECT_EQ(saved.response.proxy->password, "fresh_pass");
}
```

**Step 2: Run test to verify it fails**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter=StartupTest.RegenerateOverwritesEncryptedProxyCredentials:StartupTest.FingerprintWithCachedProfile`

Expected: FAIL because overwrite semantics and encrypted persistence are not implemented yet.

**Step 3: Write minimal implementation**

Keep the current fetch decision logic but ensure regenerated or newly fetched profiles are persisted in encrypted form and later launches reuse that encrypted cache without refetching.

**Step 4: Run test to verify it passes**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter=StartupTest.RegenerateOverwritesEncryptedProxyCredentials:StartupTest.FingerprintWithCachedProfile`

Expected: PASS

**Step 5: Commit**

```bash
git add clawbrowser/startup.cc clawbrowser/test/startup_unittest.cc
git commit -m "test: cover proxy credential refresh behavior"
```

### Task 6: Verify proxy auth and verification flows still work

**Files:**
- Modify: `clawbrowser/test/proxy_auth_login_delegate_unittest.cc`
- Modify: `clawbrowser/test/api_client_unittest.cc`
- Test: `clawbrowser/test/proxy_auth_login_delegate_unittest.cc`
- Test: `clawbrowser/test/api_client_unittest.cc`

**Step 1: Write the failing test**

Add focused tests that assert decrypted runtime credentials still reach:

- proxy auth preloading/login delegate
- proxy verification request building

**Step 2: Run test to verify it fails**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter=ProxyAuthLoginDelegateTest.*:ProxyAuthPreloaderTest.*:ApiClientTest.VerifyProxySuccess`

Expected: FAIL if any runtime consumer still assumes plaintext persisted creds.

**Step 3: Write minimal implementation**

Fix any remaining runtime consumer assumptions without expanding the storage surface or backend calls.

**Step 4: Run test to verify it passes**

Run: `autoninja -C out/Default clawbrowser_unittests && out/Default/clawbrowser_unittests --gtest_filter=ProxyAuthLoginDelegateTest.*:ProxyAuthPreloaderTest.*:ApiClientTest.VerifyProxySuccess`

Expected: PASS

**Step 5: Commit**

```bash
git add clawbrowser/test/proxy_auth_login_delegate_unittest.cc clawbrowser/test/api_client_unittest.cc
git commit -m "test: verify proxy flows with encrypted cache"
```

### Task 7: Run final verification

**Files:**
- Modify: `docs/plans/2026-04-09-proxy-credentials-cache-design.md`
- Modify: `docs/plans/2026-04-09-proxy-credentials-cache.md`

**Step 1: Run focused unit tests**

Run:

```bash
autoninja -C out/Default clawbrowser_unittests
out/Default/clawbrowser_unittests --gtest_filter=ProfileEnvelopeTest.*:ProfileManagerTest.*:FingerprintLoaderTest.*:StartupTest.*:ProxyCredentialsCryptoTest.*:ProxyAuthLoginDelegateTest.*:ProxyAuthPreloaderTest.*
```

Expected: PASS

**Step 2: Run any lightweight integration verification that is practical locally**

Run:

```bash
python3 -m pytest clawbrowser/test/integration/test_proxy.py -q
```

Expected: PASS if the local integration environment is available; otherwise document why it was skipped.

**Step 3: Update docs if implementation changed plan details**

Adjust the design or plan docs if any implementation detail differs from the approved design.

**Step 4: Commit**

```bash
git add docs/plans/2026-04-09-proxy-credentials-cache-design.md docs/plans/2026-04-09-proxy-credentials-cache.md
git commit -m "docs: finalize proxy credential cache plan"
```
