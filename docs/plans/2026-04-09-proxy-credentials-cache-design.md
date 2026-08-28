# Proxy Credential Cache Design

## Goal

Fetch fingerprint and proxy data once per profile, reuse the cached result on later launches, and stop storing proxy credentials in plaintext on disk.

## Problem

The current profile cache persists the full `GenerateResponse` to `Browser/<fp_id>/fingerprint.json`. That includes plaintext proxy `username` and `password`. Later proxy verification and auth flows read those values from runtime state, but the cache format leaves credentials exposed on disk.

The desired behavior is:

- fetch the fingerprint and proxy data once for a profile
- reuse the cached profile on later launches
- refresh the cache when a new fingerprint/profile is generated
- allow explicit force-refresh
- avoid storing proxy credentials in plaintext

## Chosen Approach

Extend the existing profile-scoped cache instead of creating a second secret store.

- Keep `Browser/<fp_id>/fingerprint.json` as the single on-disk cache for a profile.
- Remove plaintext `username` and `password` from the persisted `response.proxy`.
- Add a new encrypted blob field to the profile envelope that stores proxy credentials.
- Encrypt the credentials with AES-256-GCM using a browser-hardcoded key and a fresh random nonce per write.
- Decrypt credentials when loading a cached profile into runtime state.

This keeps the existing cache boundary, avoids extra backend requests, and limits the change to the profile persistence and load path.

## Alternatives Considered

### Separate encrypted credentials file

Rejected because it introduces a second persistence surface and extra consistency rules between the fingerprint cache and secret cache.

### Memory-only credentials cache

Rejected because it does not satisfy reuse across browser restarts.

### Keep plaintext credentials and only memoize backend fetches

Rejected because it does not address the on-disk plaintext credential exposure.

## Cache Format

The profile envelope schema will be bumped and extended with encrypted proxy credential metadata.

Persisted plaintext fields:

- `response.proxy.scheme`
- `response.proxy.host`
- `response.proxy.port`
- `response.proxy.country`
- `response.proxy.city`
- `response.proxy.connection_type`

Persisted encrypted field:

- `encrypted_proxy_credentials`

The encrypted field will contain:

- version marker
- AES-GCM nonce
- ciphertext for `{username, password}`

Backward compatibility:

- existing schema version 1 profiles with plaintext proxy creds must still parse
- old cached profiles should continue to load
- newly written profiles should use the encrypted format

## Data Flow

### New profile or force-refresh

1. Startup decides the profile needs a backend fetch because the cache is missing or refresh was requested.
2. The browser fetches the fingerprint response once.
3. Before saving the profile cache, it encrypts the proxy credentials and strips plaintext creds from the persisted proxy object.
4. The encrypted envelope is saved to `Browser/<fp_id>/fingerprint.json`.
5. The same saved file is loaded back into runtime state, including decrypted proxy credentials.

### Cached profile reuse

1. Startup sees an existing cached profile and no refresh request.
2. The browser reads the local profile cache only.
3. The loader decrypts the credential blob.
4. Runtime proxy state is hydrated with the decrypted username/password for proxy auth and verification.

## Refresh Rules

Refresh the cached encrypted proxy credentials when:

- a new profile is generated for a profile ID
- `--regenerate` or other explicit force-refresh path is used

No TTL-based refresh is added in this change.

## Error Handling

If profile decryption fails:

- treat the cached profile as invalid
- if API config is available and refresh is allowed, fetch a fresh profile and overwrite the cache
- otherwise fail startup with a clear load/decrypt error

If a profile has no proxy or incomplete proxy settings:

- keep the existing graceful behavior for proxy verification and auth preload

## Security Notes

Hardcoding the AES key in the browser is obfuscation, not strong secret protection. It is still an improvement over plaintext disk storage and matches the requested scope.

## Testing

Add or update tests for:

- profile envelope round-trip with encrypted proxy credentials
- backward-compatible parsing of older plaintext profile caches
- startup force-refresh overwriting old encrypted cache state
- runtime load path restoring decrypted proxy credentials
- existing proxy auth and verify flows continuing to work with decrypted runtime credentials
