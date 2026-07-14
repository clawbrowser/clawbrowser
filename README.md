<p align="center">
  <img src="assets/side-bite.svg" alt="Clawbrowser" width="104" />
</p>

<h1 align="center">Clawbrowser</h1>

<p align="center">
  <strong>The managed Chromium runtime for AI agents.</strong><br />
  Keep browser profiles, fingerprints, proxy routing, cookies, and storage together behind a standard CDP endpoint.
</p>

<p align="center">
  <a href="https://clawbrowser.ai/">Website</a> ·
  <a href="https://clawbrowser.ai/docs/">Documentation</a> ·
  <a href="https://app.clawbrowser.ai/">Get an API key</a> ·
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest">Releases</a> ·
  <a href="https://discord.gg/CK62brtKhe">Discord</a>
</p>

<p align="center">
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest"><img src="https://img.shields.io/github/v/release/clawbrowser/clawbrowser?label=release&color=2ea44f" alt="Latest release" /></a>
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/license-MIT-2ea44f" alt="MIT license" /></a>
  <img src="https://img.shields.io/badge/platforms-macOS%20%7C%20Linux%20%7C%20Windows-0969da" alt="macOS, Linux, and Windows" />
  <img src="https://img.shields.io/badge/protocol-CDP-0969da" alt="Chrome DevTools Protocol" />
</p>

<p align="center">
  <a href="README.md">English</a> ·
  <a href="docs/i18n/es/README.md">Español</a> ·
  <a href="docs/i18n/pt-BR/README.md">Português (Brasil)</a> ·
  <a href="docs/i18n/zh-CN/README.md">简体中文</a> ·
  <a href="docs/i18n/ja/README.md">日本語</a> ·
  <a href="docs/i18n/ko/README.md">한국어</a> ·
  <a href="docs/i18n/de/README.md">Deutsch</a> ·
  <a href="docs/i18n/fr/README.md">Français</a> ·
  <a href="docs/i18n/ru/README.md">Русский</a> ·
  <a href="docs/i18n/ar/README.md">العربية</a>
</p>

<p align="center">
  <img src="assets/clawbrowser-site-demo.gif" alt="Clawbrowser website and workflow preview" width="960" />
</p>

## Why Clawbrowser

Browser automation often breaks when browser, network, locale, and session signals do not agree. Clawbrowser manages that identity layer inside a Chromium runtime and exposes the running browser through the standard Chrome DevTools Protocol (CDP).

- Keep each named profile's fingerprint, proxy binding, cookies, and storage isolated.
- Reuse a profile when an agent needs continuity between runs.
- Connect Playwright, Puppeteer, or another CDP client to the endpoint returned by `clawctl`.
- Inspect the active profile with the built-in `clawbrowser://verify/` page.

Clawbrowser is designed to reduce interruptions caused by inconsistent browser identity signals. It is not a universal CAPTCHA bypass and does not guarantee access to every website.

## Quick Start

Get an API key at **[app.clawbrowser.ai](https://app.clawbrowser.ai/)**, then run:

```bash
clawctl install --json
clawctl config set --api-key "$CLAWBROWSER_API_KEY"
clawctl start --profile work --url https://example.com --json
clawctl endpoint --profile work --json
# Connect the returned CDP endpoint with Playwright, Puppeteer, or your agent.
```

Re-fetch the endpoint with `clawctl endpoint` after a start, restart, or failure; CDP endpoints are temporary and should not be stored in configuration.

<details>
<summary><b>Let an AI coding agent perform the installation</b></summary>

Paste the following prompt into Claude Code, Codex, Cursor, Gemini CLI, or another coding agent:

```text
Install Clawbrowser and clawctl by following the official Clawbrowser install documentation.
Primary docs:
- https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/INSTALL.md
- https://github.com/clawbrowser/clawbrowser
Instructions:
1. Read INSTALL.md first.
2. Follow the documented installation flow exactly.
3. Start from the standalone clawctl archive for the current OS/arch.
4. Do not download the browser archive manually as the bootstrap path.
5. Do not download the portable runtime manually unless INSTALL.md explicitly documents that as an offline/pre-extracted runtime path.
6. Do not use npm, npx, curl-piped installers, or a raw source checkout as the install path.
7. Run clawctl install so it can install or reuse Clawbrowser and install the portable runtime when needed.
8. Use the documented target/integration selection from INSTALL.md.
9. After installation, verify the browser using the verification steps documented in INSTALL.md.
API key:
- First check $HOME/.config/clawbrowser/config.json on Linux/macOS or %LOCALAPPDATA%\Clawbrowser\config.json on Windows.
- If api_key already exists, do not ask again.
- If api_key is missing, ask once for the real API key from https://app.clawbrowser.ai.
- Save it using the documented clawctl config command.
- Never store the API key in shell rc files, environment variables, MCP config, agent config, project files, or logs.
Expected result:
- Standalone clawctl is installed and available.
- clawctl install has completed successfully.
- Clawbrowser is installed or reused.
- The portable Linux runtime is installed only when the host requires it.
- The selected agent integration is configured according to INSTALL.md.
- clawctl start works.
- Browser verification passes according to INSTALL.md.
```

</details>

## Install

`clawctl` is the supported bootstrapper. Start with the standalone archive for the host OS and architecture from [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest). Then run `clawctl install` so it can install or reuse Clawbrowser, add the portable Linux runtime when required, and configure supported agent integrations.

Do not bootstrap from `npm`, `npx`, a curl-piped installer, Docker, a browser payload archive, or a raw source checkout.

> [!IMPORTANT]
> Do not extract `clawctl` under `/tmp`. Many agent containers mount `/tmp` with `noexec`; use a durable executable work directory instead.

<details open>
<summary><b>Linux</b> (x64 or ARM64; server, container, or no-display host)</summary>

```bash
mkdir -p ~/clawbrowser-install && cd ~/clawbrowser-install

case "$(uname -m)" in
  x86_64|amd64) platform="linux-amd64" ;;
  arm64|aarch64) platform="linux-arm64" ;;
  *) echo "unsupported architecture: $(uname -m)" >&2; exit 1 ;;
esac

archive="clawctl-${platform}.tar.gz"
url="https://github.com/clawbrowser/clawctl/releases/latest/download/${archive}"
curl -fL --retry 3 --retry-delay 2 -o "$archive" "$url"
tar -tzf "$archive" >/dev/null
tar -xzf "$archive"
cd "clawctl-${platform}"

./clawctl install --json
./clawctl config set --api-key "$CLAWBROWSER_API_KEY"
./clawctl start --profile work --url clawbrowser://verify/ --json
./clawctl endpoint --profile work --json
./clawctl verify --profile work --json
```

The portable Linux flow supports glibc `amd64` and `arm64` hosts and does not require Docker, `sudo`, `apt`, a physical display, or a manual runtime download.

</details>

<details>
<summary><b>macOS</b> (Apple Silicon)</summary>

```bash
archive="clawctl-macos-arm64.tar.gz"
url="https://github.com/clawbrowser/clawctl/releases/latest/download/${archive}"

curl -fL --retry 3 --retry-delay 2 -o "$archive" "$url"
tar -tzf "$archive" >/dev/null
tar -xzf "$archive"
cd clawctl-macos-arm64

./clawctl install --json
./clawctl config set --api-key "$CLAWBROWSER_API_KEY"
./clawctl start --profile work --url clawbrowser://verify/ --json
./clawctl endpoint --profile work --json
./clawctl verify --profile work --json
```

macOS uses `Clawbrowser.app` and requires a logged-in GUI desktop context.

</details>

<details>
<summary><b>Windows</b> (64-bit PowerShell)</summary>

```powershell
$archive = "clawctl-win-amd64.zip"
$url = "https://github.com/clawbrowser/clawctl/releases/latest/download/$archive"

Invoke-WebRequest -Uri $url -OutFile $archive
Expand-Archive -Force $archive .
Set-Location .\clawctl-win-amd64

.\clawctl.exe install --json
.\clawctl.exe config set --api-key "$env:CLAWBROWSER_API_KEY"
.\clawctl.exe start --profile work --url clawbrowser://verify/ --json
.\clawctl.exe endpoint --profile work --json
.\clawctl.exe verify --profile work --json
```

If the browser payload contains `setup.exe`, `clawctl install` runs it silently and Windows may request administrator approval.

</details>

For durable work directories, exact archive roles, offline setups, Docker-backed infrastructure, and troubleshooting, read **[INSTALL.md](./INSTALL.md)**.

## Core capabilities

| Capability | What it provides |
| --- | --- |
| Managed identity | A generated profile keeps related fingerprint surfaces together inside the browser runtime. |
| Proxy-bound profiles | Residential or datacenter routing can be associated with the generated profile. |
| Isolated sessions | Named profiles keep their own cookies, storage, identity, and endpoint. |
| Standard CDP access | Playwright, Puppeteer, and other CDP clients connect to the live browser endpoint. |
| Remote viewing | `clawctl remote` can return a temporary `dashboard_url` for watching or controlling a running profile. |
| Agent integrations | `clawctl install` writes supported integration templates into the locations used by the selected agents. |

## How it works

1. **Install the runtime.** `clawctl install` installs or reuses Clawbrowser and prepares any required portable runtime.
2. **Save authentication.** `clawctl config set --api-key …` writes the API key to Clawbrowser's configuration directory.
3. **Start a profile.** `clawctl start --profile <name>` launches managed Chromium and waits for its CDP endpoint.
4. **Verify the identity.** `clawbrowser://verify/` reports proxy egress and generated fingerprint surfaces.
5. **Connect the agent.** Read the current endpoint with `clawctl endpoint`, then connect using a standard CDP client.

## Connect a CDP client

Use the endpoint returned by `clawctl endpoint --profile work --json`.

<details open>
<summary><b>Playwright for Python</b></summary>

```python
from playwright.async_api import async_playwright

async with async_playwright() as p:
    endpoint = "http://127.0.0.1:9222"  # from clawctl endpoint
    browser = await p.chromium.connect_over_cdp(endpoint)
    page = browser.contexts[0].pages[0]
    await page.goto("https://example.com")
```

</details>

<details>
<summary><b>Playwright for Node.js</b></summary>

```js
const { chromium } = require('playwright');

const endpoint = 'http://127.0.0.1:9222'; // from clawctl endpoint
const browser = await chromium.connectOverCDP(endpoint);
const page = browser.contexts()[0].pages()[0];
await page.goto('https://example.com');
```

</details>

<details>
<summary><b>Puppeteer</b></summary>

```js
const puppeteer = require('puppeteer');

const endpoint = 'http://127.0.0.1:9222'; // from clawctl endpoint
const browser = await puppeteer.connect({ browserURL: endpoint });
const [page] = await browser.pages();
await page.goto('https://example.com');
```

</details>

> [!TIP]
> Do not override fingerprint properties through CDP. Clawbrowser applies them inside the engine; client-side overrides can create conflicting signals.

## Common workflows

<details>
<summary><b>CLI, remote viewing, and multiple profiles</b></summary>

```bash
# Start or reattach to a profile.
clawctl start --profile work --url https://example.com --json

# Always request the current endpoint after a start, restart, or failure.
clawctl endpoint --profile work --json

# Verify proxy egress and fingerprint surfaces.
clawctl verify --profile work --json

# Open a URL and create a temporary remote viewer link.
clawctl open --profile work https://example.com --json
clawctl remote --profile work --json

# Run isolated named profiles in parallel.
clawctl start --profile agent-us --url https://example.com --json
clawctl start --profile agent-de --url https://example.com --json
clawctl endpoint --profile agent-us --json
clawctl endpoint --profile agent-de --json
```

Treat a returned `dashboard_url` as a sensitive temporary control link. Reusing a profile name reuses its cached identity and session data.

</details>

## Platform support

| Platform | Runtime mode | Notes |
| --- | --- | --- |
| macOS | Native desktop app | Apple Silicon; requires a logged-in GUI desktop. |
| Linux | Portable runtime | glibc `amd64` and `arm64`; suitable for server, container, and no-display hosts. |
| Windows | Native install | 64-bit Windows through PowerShell; installation may trigger UAC. |

## Use cases

- AI-assisted research and structured data collection.
- Website QA through real browser journeys.
- Repeated monitoring workflows that need session continuity.
- Multi-profile account operations with isolated browser state.

Use Clawbrowser only on systems and data you are authorized to access, and follow the target website's terms and applicable law.

## Troubleshooting

<details>
<summary><b>Common installation and session problems</b></summary>

| Symptom | Action |
| --- | --- |
| `clawctl: command not found` | Run the binary from the extracted standalone archive or add its directory to `PATH`. |
| `Permission denied` after `chmod +x` | Re-extract outside `/tmp` or another `noexec` filesystem. |
| API key is requested repeatedly | Save it once with `clawctl config set --api-key "$CLAWBROWSER_API_KEY"`. |
| CDP endpoint is refused or stale | Run `clawctl endpoint --profile <name> --json` again after starting or restarting the profile. |
| Browser startup times out | Retry, inspect startup logs, and verify available disk space and network access. |

See **[INSTALL.md](./INSTALL.md)** for the complete installation and recovery guidance.

</details>

## Community and support

- Read the [documentation](https://clawbrowser.ai/docs/) and [FAQ](https://clawbrowser.ai/faq/).
- Report reproducible problems in [GitHub Issues](https://github.com/clawbrowser/clawbrowser/issues).
- Join the [Clawbrowser Discord](https://discord.gg/CK62brtKhe) for community discussion.
- Review product releases in [`clawbrowser/clawbrowser`](https://github.com/clawbrowser/clawbrowser/releases/latest) and bootstrapper releases in [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest).

## License

Distributed under the **MIT** license. Full text: [opensource.org/licenses/MIT](https://opensource.org/licenses/MIT).
