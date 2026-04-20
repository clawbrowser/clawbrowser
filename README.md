# ClawBrowser Release

Runtime artifacts for the ClawBrowser browser.

## Run

Docker:

```bash
docker run --rm -it \
  -e CLAWBROWSER_API_KEY=... \
  -p 9222:9222 \
  docker.io/clawbrowser/clawbrowser:latest \
  --remote-debugging-address=0.0.0.0 \
  --remote-debugging-port=9222
```

This runs headed and opens `clawbrowser://verify/`.

Standalone CLI:

```bash
./clawbrowser.real --fingerprint=my_profile --remote-debugging-port=9222
```

Useful flags:

- `--fingerprint=<id>` Explicitly select a profile ID.
- `--regenerate` Force a fresh fingerprint fetch, ignoring local cache.
- `--skip-verify` Skip the initial `clawbrowser://verify/` check.
- `--list` List cached profiles in the current config directory.
- `--output=json` Use with `--list` to print machine-readable JSON.
- `--country`, `--city`, `--connection-type` Override fingerprint targeting.
- `--verbose` Enable detailed logging.
- `--remote-debugging-address=0.0.0.0` Expose CDP outside localhost.
