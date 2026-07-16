<p align="center">
  <img src="../../../assets/side-bite.svg" alt="Clawbrowser" width="104" />
</p>

<h1 align="center">Clawbrowser</h1>

<p align="center">
  <strong>Die verwaltete Chromium-Laufzeitumgebung für KI-Agenten.</strong><br />
  Browserprofile, Fingerprints, Proxy-Routing, Cookies und Speicher bleiben gemeinsam hinter einem standardmäßigen CDP-Endpunkt verfügbar.
</p>

<p align="center">
  <a href="https://clawbrowser.ai/">Website</a> ·
  <a href="https://clawbrowser.ai/docs/">Dokumentation</a> ·
  <a href="https://app.clawbrowser.ai/">API-Schlüssel erhalten</a> ·
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest">Releases</a> ·
  <a href="https://discord.gg/DWuwhYZVn">Discord</a>
</p>

<p align="center">
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest"><img src="https://img.shields.io/github/v/release/clawbrowser/clawbrowser?label=release&color=2ea44f" alt="Neueste Version" /></a>
  <a href="../../../LICENSE"><img src="https://img.shields.io/badge/license-MIT-2ea44f" alt="MIT-Lizenz" /></a>
  <img src="https://img.shields.io/badge/platforms-macOS%20%7C%20Linux%20%7C%20Windows-0969da" alt="macOS, Linux und Windows" />
  <img src="https://img.shields.io/badge/protocol-CDP-0969da" alt="Chrome DevTools Protocol" />
</p>

<p align="center">
  <a href="../../../README.md">English</a> ·
  <a href="../es/README.md">Español</a> ·
  <a href="../pt-BR/README.md">Português (Brasil)</a> ·
  <a href="../zh-CN/README.md">简体中文</a> ·
  <a href="../ja/README.md">日本語</a> ·
  <a href="../ko/README.md">한국어</a> ·
  <a href="../de/README.md">Deutsch</a> ·
  <a href="../fr/README.md">Français</a> ·
  <a href="../ru/README.md">Русский</a> ·
  <a href="../ar/README.md">العربية</a>
</p>

<p align="center">
  <img src="../../../assets/clawbrowser-site-demo.gif" alt="Vorschau der Clawbrowser-Website und des Workflows" width="960" />
</p>

## Warum Clawbrowser

Browserautomatisierung scheitert häufig, wenn die Signale von Browser, Netzwerk, Gebietsschema und Sitzung nicht zusammenpassen. Clawbrowser verwaltet diese Identitätsebene innerhalb einer Chromium-Laufzeitumgebung und stellt den laufenden Browser über das standardmäßige Chrome DevTools Protocol (CDP) bereit.

- Fingerprint, Proxy-Bindung, Cookies und Speicher jedes benannten Profils bleiben voneinander isoliert.
- Ein Profil kann wiederverwendet werden, wenn ein Agent Kontinuität zwischen Ausführungen benötigt.
- Playwright, Puppeteer oder ein anderer CDP-Client wird mit dem von `clawctl` zurückgegebenen Endpunkt verbunden.
- Das aktive Profil kann mit der integrierten Seite `clawbrowser://verify/` überprüft werden.

Clawbrowser wurde entwickelt, um Unterbrechungen durch inkonsistente Browser-Identitätssignale zu reduzieren. Es ist keine universelle CAPTCHA-Umgehung und garantiert keinen Zugriff auf jede Website.

## Schnellstart

Rufe einen API-Schlüssel unter **[app.clawbrowser.ai](https://app.clawbrowser.ai/)** ab und führe anschließend Folgendes aus:

```bash
clawctl install --json
clawctl config set --api-key "$CLAWBROWSER_API_KEY"
clawctl start --profile work --url https://example.com --json
clawctl endpoint --profile work --json
# Connect the returned CDP endpoint with Playwright, Puppeteer, or your agent.
```

Rufe den Endpunkt nach einem Start, Neustart oder Fehler erneut mit `clawctl endpoint` ab. CDP-Endpunkte sind temporär und sollten nicht in der Konfiguration gespeichert werden.

<details>
<summary><b>Installation durch einen KI-Coding-Agenten ausführen lassen</b></summary>

Füge die folgende Eingabeaufforderung in Claude Code, Codex, Cursor, Gemini CLI oder einen anderen Coding-Agenten ein:

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

## Installation

`clawctl` ist das unterstützte Bootstrap-Programm. Beginne mit dem eigenständigen Archiv für Betriebssystem und Architektur des Hosts aus [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest). Führe dann `clawctl install` aus, damit Clawbrowser installiert oder wiederverwendet, bei Bedarf die portable Linux-Laufzeitumgebung hinzugefügt und unterstützte Agentenintegrationen konfiguriert werden können.

Verwende für den Bootstrap weder `npm`, `npx`, ein per curl weitergeleitetes Installationsprogramm, Docker, ein Browser-Payload-Archiv noch einen unverarbeiteten Quellcode-Checkout.

> [!IMPORTANT]
> Entpacke `clawctl` nicht unter `/tmp`. Viele Agentencontainer binden `/tmp` mit `noexec` ein. Verwende stattdessen ein dauerhaftes, ausführbares Arbeitsverzeichnis.

<details open>
<summary><b>Linux</b> (x64 oder ARM64; Server, Container oder Host ohne Bildschirm)</summary>

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

Der portable Linux-Ablauf unterstützt glibc-Hosts mit `amd64` und `arm64` und erfordert weder Docker, `sudo`, `apt`, einen physischen Bildschirm noch einen manuellen Laufzeit-Download.

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

macOS verwendet `Clawbrowser.app` und erfordert eine angemeldete grafische Desktop-Sitzung.

</details>

<details>
<summary><b>Windows</b> (64-Bit-PowerShell)</summary>

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

Wenn die Browser-Payload `setup.exe` enthält, führt `clawctl install` sie im Hintergrund aus. Windows kann dabei eine Administratorbestätigung anfordern.

</details>

Informationen zu dauerhaften Arbeitsverzeichnissen, genauen Archivrollen, Offline-Einrichtungen, Docker-gestützter Infrastruktur und Fehlerbehebung findest du in **[INSTALL.md](../../../INSTALL.md)**.

## Kernfunktionen

| Funktion | Bereitgestellte Leistung |
| --- | --- |
| Verwaltete Identität | Ein generiertes Profil hält zusammengehörige Fingerprint-Oberflächen innerhalb der Browser-Laufzeitumgebung zusammen. |
| Proxy-gebundene Profile | Residential- oder Datacenter-Routing kann dem generierten Profil zugeordnet werden. |
| Isolierte Sitzungen | Benannte Profile verwalten ihre eigenen Cookies, ihren Speicher, ihre Identität und ihren Endpunkt. |
| Standardmäßiger CDP-Zugriff | Playwright, Puppeteer und andere CDP-Clients verbinden sich mit dem aktiven Browser-Endpunkt. |
| Fernansicht | `clawctl remote` kann eine temporäre `dashboard_url` zum Beobachten oder Steuern eines laufenden Profils zurückgeben. |
| Agentenintegrationen | `clawctl install` schreibt unterstützte Integrationsvorlagen an die von den ausgewählten Agenten verwendeten Speicherorte. |

## Funktionsweise

1. **Laufzeitumgebung installieren.** `clawctl install` installiert Clawbrowser oder verwendet es erneut und bereitet die erforderliche portable Laufzeitumgebung vor.
2. **Authentifizierung speichern.** `clawctl config set --api-key …` schreibt den API-Schlüssel in das Konfigurationsverzeichnis von Clawbrowser.
3. **Profil starten.** `clawctl start --profile <name>` startet das verwaltete Chromium und wartet auf dessen CDP-Endpunkt.
4. **Identität überprüfen.** `clawbrowser://verify/` meldet den Proxy-Ausgang und die generierten Fingerprint-Oberflächen.
5. **Agenten verbinden.** Lies den aktuellen Endpunkt mit `clawctl endpoint` aus und stelle anschließend über einen standardmäßigen CDP-Client eine Verbindung her.

## CDP-Client verbinden

Verwende den von `clawctl endpoint --profile work --json` zurückgegebenen Endpunkt.

<details open>
<summary><b>Playwright für Python</b></summary>

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
<summary><b>Playwright für Node.js</b></summary>

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
> Überschreibe Fingerprint-Eigenschaften nicht über CDP. Clawbrowser wendet sie innerhalb der Engine an; clientseitige Überschreibungen können widersprüchliche Signale erzeugen.

## Häufige Arbeitsabläufe

<details>
<summary><b>CLI, Fernansicht und mehrere Profile</b></summary>

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

Behandle eine zurückgegebene `dashboard_url` als vertraulichen, temporären Steuerungslink. Bei Wiederverwendung eines Profilnamens werden dessen zwischengespeicherte Identitäts- und Sitzungsdaten wiederverwendet.

</details>

## Plattformunterstützung

| Plattform | Laufzeitmodus | Hinweise |
| --- | --- | --- |
| macOS | Native Desktop-App | Apple Silicon; erfordert eine angemeldete grafische Desktop-Sitzung. |
| Linux | Portable Laufzeitumgebung | glibc `amd64` und `arm64`; geeignet für Server, Container und Hosts ohne Bildschirm. |
| Windows | Native Installation | 64-Bit-Windows über PowerShell; die Installation kann UAC auslösen. |

## Anwendungsfälle

- KI-gestützte Recherche und strukturierte Datenerfassung.
- Website-Qualitätssicherung durch echte Browserabläufe.
- Wiederkehrende Überwachungsabläufe, die Sitzungskontinuität benötigen.
- Kontovorgänge mit mehreren Profilen und isoliertem Browserzustand.

Verwende Clawbrowser nur für Systeme und Daten, auf die du zugreifen darfst, und halte dich an die Bedingungen der Zielwebsite sowie das geltende Recht.

## Fehlerbehebung

<details>
<summary><b>Häufige Installations- und Sitzungsprobleme</b></summary>

| Symptom | Maßnahme |
| --- | --- |
| `clawctl: command not found` | Führe die Binärdatei aus dem entpackten eigenständigen Archiv aus oder füge ihr Verzeichnis zu `PATH` hinzu. |
| `Permission denied` nach `chmod +x` | Entpacke erneut außerhalb von `/tmp` oder einem anderen `noexec`-Dateisystem. |
| Der API-Schlüssel wird wiederholt angefordert | Speichere ihn einmal mit `clawctl config set --api-key "$CLAWBROWSER_API_KEY"`. |
| Der CDP-Endpunkt lehnt die Verbindung ab oder ist veraltet | Führe nach dem Start oder Neustart des Profils erneut `clawctl endpoint --profile <name> --json` aus. |
| Der Browserstart überschreitet das Zeitlimit | Versuche es erneut, prüfe die Startprotokolle sowie den verfügbaren Speicherplatz und Netzwerkzugriff. |

Die vollständigen Hinweise zur Installation und Wiederherstellung findest du in **[INSTALL.md](../../../INSTALL.md)**.

</details>

## Community und Support

- Lies die [Dokumentation](https://clawbrowser.ai/docs/) und die [FAQ](https://clawbrowser.ai/faq/).
- Melde reproduzierbare Probleme in [GitHub Issues](https://github.com/clawbrowser/clawbrowser/issues).
- Tritt dem [Clawbrowser Discord](https://discord.gg/DWuwhYZVn) für Diskussionen mit der Community bei.
- Prüfe Produkt-Releases in [`clawbrowser/clawbrowser`](https://github.com/clawbrowser/clawbrowser/releases/latest) und Bootstrapper-Releases in [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest).

## Lizenz

Veröffentlicht unter der **MIT**-Lizenz. Vollständiger Text: [MIT License](../../../LICENSE).
