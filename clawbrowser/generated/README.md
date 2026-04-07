# clawbrowser/generated/

This directory contains Chromium-friendly C++ types generated from the
canonical browser schema artifact at `clawbrowser/schemas/browser_schema.json`.
That artifact is extracted from `api/openapi.yaml`.

## Files

| File | Description |
|---|---|
| `fingerprint_types.h` | Generated declarations for browser-facing API schemas |
| `fingerprint_types.cc` | Generated `FromDict`/`FromJson`/`ToDict`/`ToJson` implementations |

## API contract

Every type exposes:

```cpp
static base::expected<T, std::string> T::FromDict(const base::DictValue&);
static base::expected<T, std::string> T::FromJson(const std::string&);
base::DictValue T::ToDict() const;
std::string       T::ToJson() const;
```

`FromDict`/`FromJson` return `base::unexpected(error_message)` on any required
field missing or type mismatch.  Optional fields (those absent from the OpenAPI
`required` list) are represented as `std::optional<T>` or empty `std::vector`
and do NOT cause parse failures when absent.

## Types

| C++ struct | OpenAPI schema |
|---|---|
| `Screen` | `Screen` |
| `Hardware` | `Hardware` |
| `WebGL` | `WebGL` |
| `MediaDevice` | `MediaDevice` |
| `Plugin` | `Plugin` |
| `Battery` | `Battery` |
| `ProxyConfig` | `ProxyConfig` |
| `Fingerprint` | `Fingerprint` |
| `GenerateResponse` | `GenerateResponse` |
| `ProxyCredentials` | `VerifyProxyRequest.proxy` (inline object) |
| `VerifyProxyRequest` | `VerifyProxyRequest` |
| `VerifyProxyResponse` | `VerifyProxyResponse` |

## Re-generation workflow

When the OpenAPI spec changes:

1. Re-extract the canonical schema artifact:
   ```
   python3 clawbrowser/schemas/extract_schemas.py
   ```
2. Regenerate the checked-in browser types:
   ```
   python3 clawbrowser/schemas/generate_browser_types.py \
     --schema clawbrowser/schemas/browser_schema.json \
     --header clawbrowser/generated/fingerprint_types.h \
     --source clawbrowser/generated/fingerprint_types.cc
   ```
3. Run the freshness check:
   ```
   bash clawbrowser/schemas/check_generated_fresh.sh
   ```

## Freshness check

`clawbrowser/schemas/check_generated_fresh.sh` verifies that:

- `clawbrowser/generated/fingerprint_types.{h,cc}` still match
  `clawbrowser/schemas/browser_schema.json`
- when `PyYAML` is available, `browser_schema.json` still matches
  `api/openapi.yaml`

`clawbrowser/BUILD.gn` also wires the generated-file check into the build
graph so stale generated types fail before the browser shim is considered
valid.
