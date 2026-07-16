<p align="center">
  <img src="../../../assets/side-bite.svg" alt="Clawbrowser" width="104" />
</p>

<h1 align="center">Clawbrowser</h1>

<p align="center">
  <strong>AI 에이전트를 위한 관리형 Chromium 런타임.</strong><br />
  표준 CDP 엔드포인트 뒤에서 브라우저 프로필, 핑거프린트, 프록시 라우팅, 쿠키, 스토리지를 함께 관리합니다.
</p>

<p align="center">
  <a href="https://clawbrowser.ai/">웹사이트</a> ·
  <a href="https://clawbrowser.ai/docs/">문서</a> ·
  <a href="https://app.clawbrowser.ai/">API 키 받기</a> ·
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest">릴리스</a> ·
  <a href="https://discord.gg/DWuwhYZVn">Discord</a>
</p>

<p align="center">
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest"><img src="https://img.shields.io/github/v/release/clawbrowser/clawbrowser?label=release&color=2ea44f" alt="최신 릴리스" /></a>
  <a href="../../../LICENSE"><img src="https://img.shields.io/badge/license-MIT-2ea44f" alt="MIT 라이선스" /></a>
  <img src="https://img.shields.io/badge/platforms-macOS%20%7C%20Linux%20%7C%20Windows-0969da" alt="macOS, Linux 및 Windows" />
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
  <img src="../../../assets/clawbrowser-site-demo.gif" alt="Clawbrowser 웹사이트 및 워크플로 미리보기" width="960" />
</p>

## Clawbrowser를 선택하는 이유

브라우저, 네트워크, 로캘, 세션 신호가 서로 일치하지 않으면 브라우저 자동화는 자주 중단됩니다. Clawbrowser는 Chromium 런타임 안에서 이 아이덴티티 계층을 관리하고, 실행 중인 브라우저를 표준 Chrome DevTools Protocol (CDP)을 통해 제공합니다.

- 명명된 각 프로필의 핑거프린트, 프록시 바인딩, 쿠키, 스토리지를 격리합니다.
- 에이전트가 여러 실행 사이에서 연속성을 유지해야 할 때 프로필을 재사용합니다.
- Playwright, Puppeteer 또는 다른 CDP 클라이언트를 `clawctl`이 반환한 엔드포인트에 연결합니다.
- 내장 `clawbrowser://verify/` 페이지로 활성 프로필을 확인합니다.

Clawbrowser는 일관되지 않은 브라우저 아이덴티티 신호로 인한 중단을 줄이도록 설계되었습니다. 범용 CAPTCHA 우회 수단이 아니며 모든 웹사이트에 대한 접근을 보장하지 않습니다.

## 빠른 시작

**[app.clawbrowser.ai](https://app.clawbrowser.ai/)** 에서 API 키를 받은 후 다음을 실행합니다.

```bash
clawctl install --json
clawctl config set --api-key "$CLAWBROWSER_API_KEY"
clawctl start --profile work --url https://example.com --json
clawctl endpoint --profile work --json
# Connect the returned CDP endpoint with Playwright, Puppeteer, or your agent.
```

시작, 재시작 또는 오류가 발생한 뒤에는 `clawctl endpoint`로 엔드포인트를 다시 가져오세요. CDP 엔드포인트는 임시이므로 설정에 저장해서는 안 됩니다.

<details>
<summary><b>AI 코딩 에이전트가 설치하도록 하기</b></summary>

다음 프롬프트를 Claude Code, Codex, Cursor, Gemini CLI 또는 다른 코딩 에이전트에 붙여 넣으세요.

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

## 설치

`clawctl`은 지원되는 부트스트래퍼입니다. 먼저 [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest)에서 호스트 OS 및 아키텍처에 맞는 독립 실행형 아카이브를 받으세요. 그런 다음 `clawctl install`을 실행하여 Clawbrowser를 설치하거나 재사용하고, 필요한 경우 포터블 Linux 런타임을 추가하며, 지원되는 에이전트 통합을 구성합니다.

`npm`, `npx`, curl 파이프 설치 프로그램, Docker, 브라우저 페이로드 아카이브 또는 원시 소스 체크아웃에서 부트스트랩하지 마세요.

> [!IMPORTANT]
> `clawctl`을 `/tmp` 아래에 압축 해제하지 마세요. 많은 에이전트 컨테이너가 `/tmp`를 `noexec`로 마운트하므로, 실행 가능한 영구 작업 디렉터리를 대신 사용하세요.

<details open>
<summary><b>Linux</b> (x64 또는 ARM64, 서버, 컨테이너 또는 디스플레이 없는 호스트)</summary>

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

포터블 Linux 흐름은 glibc `amd64` 및 `arm64` 호스트를 지원하며 Docker, `sudo`, `apt`, 물리적 디스플레이 또는 수동 런타임 다운로드가 필요하지 않습니다.

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

macOS는 `Clawbrowser.app`을 사용하며 로그인된 GUI 데스크톱 환경이 필요합니다.

</details>

<details>
<summary><b>Windows</b> (64비트 PowerShell)</summary>

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

브라우저 페이로드에 `setup.exe`가 포함되어 있으면 `clawctl install`이 이를 자동으로 실행하며 Windows에서 관리자 승인을 요청할 수 있습니다.

</details>

영구 작업 디렉터리, 정확한 아카이브 역할, 오프라인 설정, Docker 기반 인프라, 문제 해결은 **[INSTALL.md](../../../INSTALL.md)** 를 읽어보세요.

## 핵심 기능

| 기능 | 제공 내용 |
| --- | --- |
| 관리형 아이덴티티 | 생성된 프로필이 브라우저 런타임 안에서 관련 핑거프린트 표면을 함께 유지합니다. |
| 프록시 바인딩 프로필 | 주거용 또는 데이터 센터 라우팅을 생성된 프로필과 연결할 수 있습니다. |
| 격리된 세션 | 명명된 프로필은 각각 고유한 쿠키, 스토리지, 아이덴티티, 엔드포인트를 유지합니다. |
| 표준 CDP 접근 | Playwright, Puppeteer 및 기타 CDP 클라이언트가 실행 중인 브라우저 엔드포인트에 연결됩니다. |
| 원격 보기 | `clawctl remote`는 실행 중인 프로필을 보거나 제어할 수 있는 임시 `dashboard_url`을 반환할 수 있습니다. |
| 에이전트 통합 | `clawctl install`은 선택한 에이전트가 사용하는 위치에 지원되는 통합 템플릿을 작성합니다. |

## 작동 방식

1. **런타임을 설치합니다.** `clawctl install`은 Clawbrowser를 설치하거나 재사용하고 필요한 포터블 런타임을 준비합니다.
2. **인증을 저장합니다.** `clawctl config set --api-key …`는 API 키를 Clawbrowser 설정 디렉터리에 기록합니다.
3. **프로필을 시작합니다.** `clawctl start --profile <name>`은 관리형 Chromium을 실행하고 CDP 엔드포인트를 기다립니다.
4. **아이덴티티를 확인합니다.** `clawbrowser://verify/`는 프록시 이그레스와 생성된 핑거프린트 표면을 보고합니다.
5. **에이전트를 연결합니다.** `clawctl endpoint`로 현재 엔드포인트를 읽은 후 표준 CDP 클라이언트로 연결합니다.

## CDP 클라이언트 연결

`clawctl endpoint --profile work --json`이 반환하는 엔드포인트를 사용하세요.

<details open>
<summary><b>Python용 Playwright</b></summary>

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
<summary><b>Node.js용 Playwright</b></summary>

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
> CDP를 통해 핑거프린트 속성을 재정의하지 마세요. Clawbrowser는 엔진 내부에서 이를 적용하므로 클라이언트 측 재정의는 상충하는 신호를 만들 수 있습니다.

## 일반적인 워크플로

<details>
<summary><b>CLI, 원격 보기 및 여러 프로필</b></summary>

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

반환된 `dashboard_url`은 민감한 임시 제어 링크로 취급하세요. 프로필 이름을 재사용하면 캐시된 아이덴티티와 세션 데이터도 재사용됩니다.

</details>

## 플랫폼 지원

| 플랫폼 | 런타임 모드 | 참고 |
| --- | --- | --- |
| macOS | 네이티브 데스크톱 앱 | Apple Silicon, 로그인된 GUI 데스크톱이 필요합니다. |
| Linux | 포터블 런타임 | glibc `amd64` 및 `arm64`, 서버, 컨테이너, 디스플레이 없는 호스트에 적합합니다. |
| Windows | 네이티브 설치 | PowerShell을 사용하는 64비트 Windows, 설치 시 UAC가 실행될 수 있습니다. |

## 사용 사례

- AI 지원 리서치 및 구조화된 데이터 수집.
- 실제 브라우저 경로를 통한 웹사이트 QA.
- 세션 연속성이 필요한 반복 모니터링 워크플로.
- 격리된 브라우저 상태를 사용하는 다중 프로필 계정 작업.

접근 권한이 있는 시스템과 데이터에서만 Clawbrowser를 사용하고 대상 웹사이트의 약관 및 관련 법률을 준수하세요.

## 문제 해결

<details>
<summary><b>일반적인 설치 및 세션 문제</b></summary>

| 증상 | 해결 방법 |
| --- | --- |
| `clawctl: command not found` | 압축 해제한 독립 실행형 아카이브에서 바이너리를 실행하거나 해당 디렉터리를 `PATH`에 추가하세요. |
| `chmod +x` 후 `Permission denied` 발생 | `/tmp` 또는 다른 `noexec` 파일 시스템 밖에 다시 압축 해제하세요. |
| API 키를 반복해서 요청함 | `clawctl config set --api-key "$CLAWBROWSER_API_KEY"`로 한 번 저장하세요. |
| CDP 엔드포인트 연결이 거부되거나 오래됨 | 프로필을 시작하거나 재시작한 후 `clawctl endpoint --profile <name> --json`을 다시 실행하세요. |
| 브라우저 시작 시간 초과 | 다시 시도하고 시작 로그를 확인하며 사용 가능한 디스크 공간과 네트워크 접근을 점검하세요. |

전체 설치 및 복구 안내는 **[INSTALL.md](../../../INSTALL.md)** 를 참조하세요.

</details>

## 커뮤니티 및 지원

- [문서](https://clawbrowser.ai/docs/)와 [FAQ](https://clawbrowser.ai/faq/)를 읽어보세요.
- 재현 가능한 문제는 [GitHub Issues](https://github.com/clawbrowser/clawbrowser/issues)에 보고하세요.
- 커뮤니티 토론을 위해 [Clawbrowser Discord](https://discord.gg/DWuwhYZVn)에 참여하세요.
- 제품 릴리스는 [`clawbrowser/clawbrowser`](https://github.com/clawbrowser/clawbrowser/releases/latest)에서, 부트스트래퍼 릴리스는 [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest)에서 확인하세요.

## 라이선스

**MIT** 라이선스에 따라 배포됩니다. 전문: [MIT License](../../../LICENSE).
