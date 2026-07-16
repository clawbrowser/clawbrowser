<p align="center">
  <img src="../../../assets/side-bite.svg" alt="Clawbrowser" width="104" />
</p>

<h1 align="center">Clawbrowser</h1>

<p align="center">
  <strong>AI エージェント向けのマネージド Chromium ランタイム。</strong><br />
  ブラウザプロファイル、フィンガープリント、プロキシルーティング、Cookie、ストレージを、標準の CDP エンドポイントの背後でまとめて管理します。
</p>

<p align="center">
  <a href="https://clawbrowser.ai/">ウェブサイト</a> ·
  <a href="https://clawbrowser.ai/docs/">ドキュメント</a> ·
  <a href="https://app.clawbrowser.ai/">API キーを取得</a> ·
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest">リリース</a> ·
  <a href="https://discord.gg/DWuwhYZVn">Discord</a>
</p>

<p align="center">
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest"><img src="https://img.shields.io/github/v/release/clawbrowser/clawbrowser?label=release&color=2ea44f" alt="最新リリース" /></a>
  <a href="../../../LICENSE"><img src="https://img.shields.io/badge/license-MIT-2ea44f" alt="MIT ライセンス" /></a>
  <img src="https://img.shields.io/badge/platforms-macOS%20%7C%20Linux%20%7C%20Windows-0969da" alt="macOS、Linux、Windows" />
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
  <img src="../../../assets/clawbrowser-site-demo.gif" alt="Clawbrowser のウェブサイトとワークフローのプレビュー" width="960" />
</p>

## Clawbrowser を選ぶ理由

ブラウザ、ネットワーク、ロケール、セッションの各シグナルが一致しないと、ブラウザ自動化はしばしば機能しなくなります。Clawbrowser は Chromium ランタイム内でそのアイデンティティレイヤーを管理し、実行中のブラウザを標準の Chrome DevTools Protocol (CDP) 経由で公開します。

- 名前付きプロファイルごとに、フィンガープリント、プロキシの関連付け、Cookie、ストレージを分離します。
- エージェントが複数回の実行にわたって継続性を必要とする場合は、プロファイルを再利用します。
- Playwright、Puppeteer、または別の CDP クライアントを `clawctl` が返すエンドポイントに接続します。
- 組み込みの `clawbrowser://verify/` ページでアクティブなプロファイルを確認します。

Clawbrowser は、ブラウザのアイデンティティシグナルの不整合による中断を減らすよう設計されています。万能な CAPTCHA 回避手段ではなく、あらゆるウェブサイトへのアクセスを保証するものでもありません。

## クイックスタート

**[app.clawbrowser.ai](https://app.clawbrowser.ai/)** で API キーを取得し、次を実行します。

```bash
clawctl install --json
clawctl config set --api-key "$CLAWBROWSER_API_KEY"
clawctl start --profile work --url https://example.com --json
clawctl endpoint --profile work --json
# Connect the returned CDP endpoint with Playwright, Puppeteer, or your agent.
```

起動、再起動、または障害の後は `clawctl endpoint` でエンドポイントを再取得してください。CDP エンドポイントは一時的なため、設定に保存しないでください。

<details>
<summary><b>AI コーディングエージェントにインストールを実行させる</b></summary>

次のプロンプトを Claude Code、Codex、Cursor、Gemini CLI、または別のコーディングエージェントに貼り付けます。

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

## インストール

`clawctl` はサポート対象のブートストラッパーです。まず [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest) から、ホストの OS とアーキテクチャに対応するスタンドアロンアーカイブを取得します。次に `clawctl install` を実行すると、Clawbrowser のインストールまたは再利用、必要に応じたポータブル Linux ランタイムの追加、サポート対象エージェント統合の設定が行われます。

`npm`、`npx`、curl の出力をパイプするインストーラー、Docker、ブラウザペイロードのアーカイブ、または未加工のソースチェックアウトをブートストラップに使用しないでください。

> [!IMPORTANT]
> `clawctl` を `/tmp` 以下に展開しないでください。多くのエージェントコンテナでは `/tmp` が `noexec` でマウントされています。代わりに、永続的で実行可能な作業ディレクトリを使用してください。

<details open>
<summary><b>Linux</b>（x64 または ARM64、サーバー、コンテナ、ディスプレイのないホスト）</summary>

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

ポータブル Linux フローは glibc の `amd64` および `arm64` ホストをサポートし、Docker、`sudo`、`apt`、物理ディスプレイ、手動でのランタイムダウンロードを必要としません。

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

macOS では `Clawbrowser.app` を使用し、ログイン済みの GUI デスクトップ環境が必要です。

</details>

<details>
<summary><b>Windows</b>（64 ビット PowerShell）</summary>

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

ブラウザペイロードに `setup.exe` が含まれている場合、`clawctl install` はそれをサイレント実行し、Windows が管理者の承認を求めることがあります。

</details>

永続的な作業ディレクトリ、各アーカイブの正確な役割、オフラインセットアップ、Docker ベースのインフラストラクチャ、トラブルシューティングについては、**[INSTALL.md](../../../INSTALL.md)** を参照してください。

## 主な機能

| 機能 | 提供内容 |
| --- | --- |
| マネージドアイデンティティ | 生成されたプロファイルが、関連するフィンガープリントサーフェスをブラウザランタイム内でまとめて維持します。 |
| プロキシに関連付けられたプロファイル | 住宅用またはデータセンターのルーティングを、生成されたプロファイルに関連付けられます。 |
| 分離されたセッション | 名前付きプロファイルが、それぞれ独自の Cookie、ストレージ、アイデンティティ、エンドポイントを保持します。 |
| 標準 CDP アクセス | Playwright、Puppeteer、その他の CDP クライアントが稼働中のブラウザエンドポイントに接続します。 |
| リモート表示 | `clawctl remote` は、実行中のプロファイルを表示または操作するための一時的な `dashboard_url` を返せます。 |
| エージェント統合 | `clawctl install` は、選択したエージェントが使用する場所に、サポート対象の統合テンプレートを書き込みます。 |

## 仕組み

1. **ランタイムをインストールします。** `clawctl install` は Clawbrowser をインストールまたは再利用し、必要なポータブルランタイムを準備します。
2. **認証情報を保存します。** `clawctl config set --api-key …` は API キーを Clawbrowser の設定ディレクトリに書き込みます。
3. **プロファイルを起動します。** `clawctl start --profile <name>` はマネージド Chromium を起動し、その CDP エンドポイントを待機します。
4. **アイデンティティを検証します。** `clawbrowser://verify/` はプロキシの出口と生成されたフィンガープリントサーフェスを報告します。
5. **エージェントを接続します。** `clawctl endpoint` で現在のエンドポイントを取得し、標準の CDP クライアントで接続します。

## CDP クライアントを接続する

`clawctl endpoint --profile work --json` が返すエンドポイントを使用します。

<details open>
<summary><b>Python 向け Playwright</b></summary>

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
<summary><b>Node.js 向け Playwright</b></summary>

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
> CDP 経由でフィンガープリントプロパティを上書きしないでください。Clawbrowser はエンジン内部で適用するため、クライアント側の上書きによって矛盾するシグナルが生じる可能性があります。

## 一般的なワークフロー

<details>
<summary><b>CLI、リモート表示、複数プロファイル</b></summary>

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

返された `dashboard_url` は機密性のある一時的な操作リンクとして扱ってください。同じプロファイル名を再利用すると、キャッシュされたアイデンティティとセッションデータも再利用されます。

</details>

## 対応プラットフォーム

| プラットフォーム | ランタイムモード | 備考 |
| --- | --- | --- |
| macOS | ネイティブデスクトップアプリ | Apple Silicon。ログイン済みの GUI デスクトップが必要です。 |
| Linux | ポータブルランタイム | glibc `amd64` および `arm64`。サーバー、コンテナ、ディスプレイのないホストに適しています。 |
| Windows | ネイティブインストール | PowerShell 経由の 64 ビット Windows。インストール時に UAC が表示されることがあります。 |

## ユースケース

- AI を活用した調査と構造化データ収集。
- 実際のブラウザ操作を通じたウェブサイトの QA。
- セッションの継続性が必要な反復監視ワークフロー。
- ブラウザ状態を分離した複数プロファイルでのアカウント操作。

Clawbrowser は、アクセスを許可されたシステムとデータに対してのみ使用し、対象ウェブサイトの利用規約と適用法を遵守してください。

## トラブルシューティング

<details>
<summary><b>一般的なインストールとセッションの問題</b></summary>

| 症状 | 対処方法 |
| --- | --- |
| `clawctl: command not found` | 展開したスタンドアロンアーカイブからバイナリを実行するか、そのディレクトリを `PATH` に追加します。 |
| `chmod +x` 後に `Permission denied` が表示される | `/tmp` または別の `noexec` ファイルシステムの外に再展開します。 |
| API キーを繰り返し求められる | `clawctl config set --api-key "$CLAWBROWSER_API_KEY"` で一度保存します。 |
| CDP エンドポイントが接続を拒否する、または古い | プロファイルの起動または再起動後に、`clawctl endpoint --profile <name> --json` を再実行します。 |
| ブラウザの起動がタイムアウトする | 再試行し、起動ログを確認して、利用可能なディスク容量とネットワークアクセスを検証します。 |

完全なインストールと復旧のガイドは **[INSTALL.md](../../../INSTALL.md)** を参照してください。

</details>

## コミュニティとサポート

- [ドキュメント](https://clawbrowser.ai/docs/)と [FAQ](https://clawbrowser.ai/faq/) を参照してください。
- 再現可能な問題は [GitHub Issues](https://github.com/clawbrowser/clawbrowser/issues) で報告してください。
- コミュニティでの議論には [Clawbrowser Discord](https://discord.gg/DWuwhYZVn) へ参加してください。
- 製品リリースは [`clawbrowser/clawbrowser`](https://github.com/clawbrowser/clawbrowser/releases/latest)、ブートストラッパーのリリースは [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest) で確認できます。

## ライセンス

**MIT** ライセンスの下で配布されています。全文：[MIT License](../../../LICENSE)。
