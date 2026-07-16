<p align="center">
  <img src="../../../assets/side-bite.svg" alt="Clawbrowser" width="104" />
</p>

<h1 align="center">Clawbrowser</h1>

<p align="center">
  <strong>面向 AI 智能体的托管式 Chromium 运行时。</strong><br />
  通过标准 CDP 端点，将浏览器配置文件、指纹、代理路由、Cookie 和存储统一管理。
</p>

<p align="center">
  <a href="https://clawbrowser.ai/">网站</a> ·
  <a href="https://clawbrowser.ai/docs/">文档</a> ·
  <a href="https://app.clawbrowser.ai/">获取 API 密钥</a> ·
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest">版本发布</a> ·
  <a href="https://discord.gg/DWuwhYZVn">Discord</a>
</p>

<p align="center">
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest"><img src="https://img.shields.io/github/v/release/clawbrowser/clawbrowser?label=release&color=2ea44f" alt="最新版本" /></a>
  <a href="../../../LICENSE"><img src="https://img.shields.io/badge/license-MIT-2ea44f" alt="MIT 许可证" /></a>
  <img src="https://img.shields.io/badge/platforms-macOS%20%7C%20Linux%20%7C%20Windows-0969da" alt="macOS、Linux 和 Windows" />
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
  <img src="../../../assets/clawbrowser-site-demo.gif" alt="Clawbrowser 网站和工作流程预览" width="960" />
</p>

## 为什么选择 Clawbrowser

当浏览器、网络、区域设置和会话信号不一致时，浏览器自动化往往会中断。Clawbrowser 在 Chromium 运行时内管理这一身份层，并通过标准 Chrome DevTools Protocol (CDP) 暴露正在运行的浏览器。

- 隔离每个命名配置文件的指纹、代理绑定、Cookie 和存储。
- 当智能体需要在多次运行之间保持连续性时，复用配置文件。
- 将 Playwright、Puppeteer 或其他 CDP 客户端连接到 `clawctl` 返回的端点。
- 使用内置的 `clawbrowser://verify/` 页面检查活动配置文件。

Clawbrowser 旨在减少因浏览器身份信号不一致而导致的中断。它不是通用的 CAPTCHA 绕过工具，也不保证能够访问所有网站。

## 快速开始

在 **[app.clawbrowser.ai](https://app.clawbrowser.ai/)** 获取 API 密钥，然后运行：

```bash
clawctl install --json
clawctl config set --api-key "$CLAWBROWSER_API_KEY"
clawctl start --profile work --url https://example.com --json
clawctl endpoint --profile work --json
# Connect the returned CDP endpoint with Playwright, Puppeteer, or your agent.
```

每次启动、重启或发生故障后，请使用 `clawctl endpoint` 重新获取端点；CDP 端点是临时的，不应存入配置。

<details>
<summary><b>让 AI 编程智能体执行安装</b></summary>

将以下提示粘贴到 Claude Code、Codex、Cursor、Gemini CLI 或其他编程智能体中：

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

## 安装

`clawctl` 是受支持的引导安装程序。首先从 [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest) 获取适用于主机操作系统和架构的独立归档包。然后运行 `clawctl install`，使其安装或复用 Clawbrowser，在需要时添加便携式 Linux 运行时，并配置受支持的智能体集成。

请勿使用 `npm`、`npx`、通过管道传给 shell 的 curl 安装程序、Docker、浏览器负载归档包或原始源码检出作为引导安装方式。

> [!IMPORTANT]
> 请勿将 `clawctl` 解压到 `/tmp` 下。许多智能体容器会以 `noexec` 方式挂载 `/tmp`；请改用持久且允许执行的工作目录。

<details open>
<summary><b>Linux</b>（x64 或 ARM64；服务器、容器或无显示器主机）</summary>

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

便携式 Linux 流程支持 glibc `amd64` 和 `arm64` 主机，且不需要 Docker、`sudo`、`apt`、物理显示器或手动下载运行时。

</details>

<details>
<summary><b>macOS</b>（Apple Silicon）</summary>

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

macOS 使用 `Clawbrowser.app`，并且需要已登录的 GUI 桌面环境。

</details>

<details>
<summary><b>Windows</b>（64 位 PowerShell）</summary>

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

如果浏览器负载包含 `setup.exe`，`clawctl install` 会以静默方式运行它，Windows 可能会请求管理员批准。

</details>

有关持久工作目录、各归档包的确切用途、离线设置、Docker 支持的基础设施和故障排除，请阅读 **[INSTALL.md](../../../INSTALL.md)**。

## 核心能力

| 能力 | 提供的功能 |
| --- | --- |
| 托管身份 | 生成的配置文件在浏览器运行时内统一维护相关的指纹表面。 |
| 代理绑定配置文件 | 可将住宅或数据中心路由与生成的配置文件关联。 |
| 隔离会话 | 命名配置文件分别保留自己的 Cookie、存储、身份和端点。 |
| 标准 CDP 访问 | Playwright、Puppeteer 和其他 CDP 客户端连接到活动浏览器端点。 |
| 远程查看 | `clawctl remote` 可返回临时 `dashboard_url`，用于查看或控制正在运行的配置文件。 |
| 智能体集成 | `clawctl install` 将受支持的集成模板写入所选智能体使用的位置。 |

## 工作原理

1. **安装运行时。** `clawctl install` 安装或复用 Clawbrowser，并准备所有必需的便携式运行时。
2. **保存身份验证。** `clawctl config set --api-key …` 将 API 密钥写入 Clawbrowser 的配置目录。
3. **启动配置文件。** `clawctl start --profile <name>` 启动托管式 Chromium，并等待其 CDP 端点。
4. **验证身份。** `clawbrowser://verify/` 报告代理出口和生成的指纹表面。
5. **连接智能体。** 使用 `clawctl endpoint` 读取当前端点，然后通过标准 CDP 客户端连接。

## 连接 CDP 客户端

使用 `clawctl endpoint --profile work --json` 返回的端点。

<details open>
<summary><b>Python 版 Playwright</b></summary>

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
<summary><b>Node.js 版 Playwright</b></summary>

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
> 请勿通过 CDP 覆盖指纹属性。Clawbrowser 会在引擎内部应用这些属性；客户端覆盖可能产生相互冲突的信号。

## 常见工作流程

<details>
<summary><b>CLI、远程查看和多个配置文件</b></summary>

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

请将返回的 `dashboard_url` 视为敏感的临时控制链接。复用配置文件名称会复用其缓存的身份和会话数据。

</details>

## 平台支持

| 平台 | 运行时模式 | 说明 |
| --- | --- | --- |
| macOS | 原生桌面应用 | Apple Silicon；需要已登录的 GUI 桌面。 |
| Linux | 便携式运行时 | glibc `amd64` 和 `arm64`；适用于服务器、容器和无显示器主机。 |
| Windows | 原生安装 | 通过 PowerShell 在 64 位 Windows 上安装；安装可能触发 UAC。 |

## 使用场景

- AI 辅助研究和结构化数据收集。
- 通过真实浏览器流程进行网站质量保证。
- 需要会话连续性的重复监控工作流程。
- 使用隔离浏览器状态的多配置文件账户操作。

仅在您获授权访问的系统和数据上使用 Clawbrowser，并遵守目标网站的条款和适用法律。

## 故障排除

<details>
<summary><b>常见安装和会话问题</b></summary>

| 症状 | 处理方法 |
| --- | --- |
| `clawctl: command not found` | 从解压后的独立归档包中运行二进制文件，或将其目录添加到 `PATH`。 |
| 执行 `chmod +x` 后出现 `Permission denied` | 在 `/tmp` 或其他 `noexec` 文件系统之外重新解压。 |
| 反复要求输入 API 密钥 | 使用 `clawctl config set --api-key "$CLAWBROWSER_API_KEY"` 保存一次。 |
| CDP 端点拒绝连接或已失效 | 启动或重启配置文件后，再次运行 `clawctl endpoint --profile <name> --json`。 |
| 浏览器启动超时 | 重试、检查启动日志，并确认有足够的磁盘空间和网络访问。 |

完整的安装和恢复指南请参阅 **[INSTALL.md](../../../INSTALL.md)**。

</details>

## 社区与支持

- 阅读[文档](https://clawbrowser.ai/docs/)和[常见问题](https://clawbrowser.ai/faq/)。
- 在 [GitHub Issues](https://github.com/clawbrowser/clawbrowser/issues) 中报告可复现的问题。
- 加入 [Clawbrowser Discord](https://discord.gg/DWuwhYZVn) 参与社区讨论。
- 在 [`clawbrowser/clawbrowser`](https://github.com/clawbrowser/clawbrowser/releases/latest) 查看产品版本，在 [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest) 查看引导程序版本。

## 许可证

本项目采用 **MIT** 许可证分发。完整文本：[MIT License](../../../LICENSE)。
