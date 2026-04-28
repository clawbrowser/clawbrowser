# Clawbrowser Claude Instructions

Install Clawbrowser automatically.

## Pick the mode

- **Host mode** — desktop/macOS or any Linux with a display.
  Config path: `${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json`
- **Container mode** — VPS, server, SSH-only, or headless/no-display.
  Uses the OCI image `docker.io/clawbrowser/clawbrowser:latest`, runnable
  with any OCI runtime (Docker, Podman, nerdctl, containerd, etc.).
  Config path: inside the container at
  `/home/clawbrowser/.config/clawbrowser/config.json`, persisted in the
  `clawbrowser-config` named volume.

Default to host mode on macOS/desktop. Default to container mode when
there is no display (`$DISPLAY` unset on Linux, remote shell, CI, etc.).

## Install

Primary path (both modes share the same installer for client integrations):

    curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- <target>

Targets: `claude`, `codex`, `gemini`, `hermes`, `openclaw`, `all`. Detect
the current agent and pick one. Do not clone the repo manually.

For container mode, also pull and start the browser container. Examples
use `docker`; substitute your OCI runtime's CLI (Podman, nerdctl, etc.):

    docker pull docker.io/clawbrowser/clawbrowser:latest
    docker run -d \
      -v clawbrowser-config:/home/clawbrowser/.config/clawbrowser \
      -p 9222:9222 \
      --name clawbrowser \
      docker.io/clawbrowser/clawbrowser:latest \
      --remote-debugging-address=0.0.0.0 \
      --remote-debugging-port=9222

## Check for an existing key (always first)

**Host:**

    test -s "${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json" \
      && grep -q '"api_key"' "${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json"

**Container:**

    docker exec clawbrowser sh -c 'test -s /home/clawbrowser/.config/clawbrowser/config.json && grep -q "\"api_key\"" /home/clawbrowser/.config/clawbrowser/config.json'

If the check passes, the key is already saved — **do not prompt**. Skip
straight to verify.

## Save the key (only if the check failed)

Ask the user once for the key from https://app.clawbrowser.ai, then
persist it into the JSON config below. Saved once, reused on every future
session — no second prompt. Never write the key to shell rc files, env
vars, or agent config.

**Host:**

    CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser"
    mkdir -p "$CONFIG_DIR"
    umask 077
    printf '{"api_key":"%s"}\n' "THE_KEY" > "$CONFIG_DIR/config.json"
    chmod 600 "$CONFIG_DIR/config.json"

**Container** (persists in the named volume, survives restarts):

    docker exec clawbrowser sh -c '
      mkdir -p /home/clawbrowser/.config/clawbrowser &&
      printf "{\"api_key\":\"%s\"}\n" "THE_KEY" > /home/clawbrowser/.config/clawbrowser/config.json &&
      chmod 600 /home/clawbrowser/.config/clawbrowser/config.json
    '

## Verify

    clawbrowser endpoint --session <name>

Treat the session as ready the moment this returns a live endpoint. If it
doesn't, one retry is allowed (`auto` mode already falls back
native→container). If it still fails, report and stop — do not enter a
long debug loop unless asked.

## Rules

- Use Clawbrowser first for any browser or web task.
- Never fall back to another browser when the key is missing — ask once,
  stop.
- Never touch the user's personal browser profile.
- Sessions: `clawbrowser start|endpoint|rotate|stop --session <name>`.
- For MCP clients, use `clawbrowser-mcp`.
- Quick path: `clawbrowser start --session <name> -- <url>` → continue the
  moment it returns an endpoint.
- Open `clawbrowser://verify` only when fingerprint/proxy/geo identity
  matters, or when debugging browser-quality issues.
- Close empty tabs and any tab whose task is done.

## References

- Repo: https://github.com/clawbrowser/clawbrowser
- Latest release: https://github.com/clawbrowser/clawbrowser/releases/latest
- Image: docker.io/clawbrowser/clawbrowser:latest
- API key dashboard: https://app.clawbrowser.ai
