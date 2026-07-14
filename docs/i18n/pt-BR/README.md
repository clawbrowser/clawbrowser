<p align="center">
  <img src="../../../assets/side-bite.svg" alt="Clawbrowser" width="104" />
</p>

<h1 align="center">Clawbrowser</h1>

<p align="center">
  <strong>O runtime Chromium gerenciado para agentes de IA.</strong><br />
  Mantenha perfis do navegador, impressões digitais, roteamento de proxy, cookies e armazenamento juntos por trás de um endpoint CDP padrão.
</p>

<p align="center">
  <a href="https://clawbrowser.ai/">Site</a> ·
  <a href="https://clawbrowser.ai/docs/">Documentação</a> ·
  <a href="https://app.clawbrowser.ai/">Obter uma chave de API</a> ·
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest">Versões</a> ·
  <a href="https://discord.gg/CK62brtKhe">Discord</a>
</p>

<p align="center">
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest"><img src="https://img.shields.io/github/v/release/clawbrowser/clawbrowser?label=release&color=2ea44f" alt="Versão mais recente" /></a>
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/license-MIT-2ea44f" alt="Licença MIT" /></a>
  <img src="https://img.shields.io/badge/platforms-macOS%20%7C%20Linux%20%7C%20Windows-0969da" alt="macOS, Linux e Windows" />
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
  <img src="../../../assets/clawbrowser-site-demo.gif" alt="Prévia do site e do fluxo de trabalho do Clawbrowser" width="960" />
</p>

## Por que Clawbrowser

A automação de navegador costuma falhar quando os sinais do navegador, da rede, da localidade e da sessão não estão de acordo. O Clawbrowser gerencia essa camada de identidade dentro de um runtime Chromium e expõe o navegador em execução por meio do padrão Chrome DevTools Protocol (CDP).

- Mantenha isolados a impressão digital, a vinculação de proxy, os cookies e o armazenamento de cada perfil nomeado.
- Reutilize um perfil quando um agente precisar de continuidade entre execuções.
- Conecte Playwright, Puppeteer ou outro cliente CDP ao endpoint retornado pelo `clawctl`.
- Inspecione o perfil ativo com a página integrada `clawbrowser://verify/`.

O Clawbrowser foi projetado para reduzir interrupções causadas por sinais inconsistentes de identidade do navegador. Ele não é uma solução universal para contornar CAPTCHA e não garante acesso a todos os sites.

## Início rápido

Obtenha uma chave de API em **[app.clawbrowser.ai](https://app.clawbrowser.ai/)** e execute:

```bash
clawctl install --json
clawctl config set --api-key "$CLAWBROWSER_API_KEY"
clawctl start --profile work --url https://example.com --json
clawctl endpoint --profile work --json
# Connect the returned CDP endpoint with Playwright, Puppeteer, or your agent.
```

Busque novamente o endpoint com `clawctl endpoint` após uma inicialização, reinicialização ou falha; os endpoints CDP são temporários e não devem ser armazenados na configuração.

<details>
<summary><b>Permitir que um agente de programação com IA faça a instalação</b></summary>

Cole o prompt a seguir no Claude Code, Codex, Cursor, Gemini CLI ou em outro agente de programação:

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

## Instalação

O `clawctl` é o bootstrapper compatível. Comece com o arquivo independente para o sistema operacional e a arquitetura do host em [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest). Em seguida, execute `clawctl install` para que ele possa instalar ou reutilizar o Clawbrowser, adicionar o runtime portátil do Linux quando necessário e configurar as integrações de agentes compatíveis.

Não faça o bootstrap a partir de `npm`, `npx`, um instalador canalizado por curl, Docker, um arquivo de payload do navegador ou um checkout do código-fonte bruto.

> [!IMPORTANT]
> Não extraia o `clawctl` em `/tmp`. Muitos contêineres de agentes montam `/tmp` com `noexec`; use um diretório de trabalho executável e durável.

<details open>
<summary><b>Linux</b> (x64 ou ARM64; servidor, contêiner ou host sem tela)</summary>

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

O fluxo portátil do Linux é compatível com hosts glibc `amd64` e `arm64` e não requer Docker, `sudo`, `apt`, uma tela física ou o download manual do runtime.

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

O macOS usa o `Clawbrowser.app` e requer um contexto de desktop com GUI e uma sessão iniciada.

</details>

<details>
<summary><b>Windows</b> (PowerShell de 64 bits)</summary>

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

Se o payload do navegador contiver `setup.exe`, o `clawctl install` o executará silenciosamente, e o Windows poderá solicitar aprovação de administrador.

</details>

Para diretórios de trabalho duráveis, funções exatas dos arquivos, configurações offline, infraestrutura apoiada por Docker e solução de problemas, leia **[INSTALL.md](../../../INSTALL.md)**.

## Principais recursos

| Recurso | O que ele oferece |
| --- | --- |
| Identidade gerenciada | Um perfil gerado mantém juntas as superfícies de impressão digital relacionadas dentro do runtime do navegador. |
| Perfis vinculados a proxy | O roteamento residencial ou de datacenter pode ser associado ao perfil gerado. |
| Sessões isoladas | Perfis nomeados mantêm seus próprios cookies, armazenamento, identidade e endpoint. |
| Acesso CDP padrão | Playwright, Puppeteer e outros clientes CDP se conectam ao endpoint do navegador ativo. |
| Visualização remota | `clawctl remote` pode retornar uma `dashboard_url` temporária para assistir ou controlar um perfil em execução. |
| Integrações de agentes | `clawctl install` grava os modelos de integração compatíveis nos locais usados pelos agentes selecionados. |

## Como funciona

1. **Instale o runtime.** `clawctl install` instala ou reutiliza o Clawbrowser e prepara qualquer runtime portátil necessário.
2. **Salve a autenticação.** `clawctl config set --api-key …` grava a chave de API no diretório de configuração do Clawbrowser.
3. **Inicie um perfil.** `clawctl start --profile <name>` inicia o Chromium gerenciado e aguarda seu endpoint CDP.
4. **Verifique a identidade.** `clawbrowser://verify/` informa a saída do proxy e as superfícies de impressão digital geradas.
5. **Conecte o agente.** Leia o endpoint atual com `clawctl endpoint` e conecte-se usando um cliente CDP padrão.

## Conectar um cliente CDP

Use o endpoint retornado por `clawctl endpoint --profile work --json`.

<details open>
<summary><b>Playwright para Python</b></summary>

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
<summary><b>Playwright para Node.js</b></summary>

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
> Não substitua as propriedades de impressão digital por meio do CDP. O Clawbrowser as aplica dentro do mecanismo; substituições do lado do cliente podem criar sinais conflitantes.

## Fluxos de trabalho comuns

<details>
<summary><b>CLI, visualização remota e vários perfis</b></summary>

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

Trate a `dashboard_url` retornada como um link temporário de controle confidencial. Reutilizar o nome de um perfil reutiliza sua identidade e seus dados de sessão em cache.

</details>

## Compatibilidade com plataformas

| Plataforma | Modo de runtime | Observações |
| --- | --- | --- |
| macOS | Aplicativo de desktop nativo | Apple Silicon; requer um desktop com GUI e uma sessão iniciada. |
| Linux | Runtime portátil | glibc `amd64` e `arm64`; adequado para servidores, contêineres e hosts sem tela. |
| Windows | Instalação nativa | Windows de 64 bits por meio do PowerShell; a instalação pode acionar o UAC. |

## Casos de uso

- Pesquisa assistida por IA e coleta de dados estruturados.
- QA de sites por meio de jornadas reais no navegador.
- Fluxos de monitoramento repetidos que precisam de continuidade de sessão.
- Operações de contas com vários perfis e estado isolado do navegador.

Use o Clawbrowser somente em sistemas e dados que você tenha autorização para acessar e siga os termos do site de destino e a legislação aplicável.

## Solução de problemas

<details>
<summary><b>Problemas comuns de instalação e sessão</b></summary>

| Sintoma | Ação |
| --- | --- |
| `clawctl: command not found` | Execute o binário a partir do arquivo independente extraído ou adicione seu diretório ao `PATH`. |
| `Permission denied` após `chmod +x` | Extraia novamente fora de `/tmp` ou de outro sistema de arquivos com `noexec`. |
| A chave de API é solicitada repetidamente | Salve-a uma vez com `clawctl config set --api-key "$CLAWBROWSER_API_KEY"`. |
| O endpoint CDP é recusado ou está desatualizado | Execute `clawctl endpoint --profile <name> --json` novamente após iniciar ou reiniciar o perfil. |
| A inicialização do navegador atinge o tempo limite | Tente novamente, inspecione os logs de inicialização e verifique o espaço disponível em disco e o acesso à rede. |

Consulte **[INSTALL.md](../../../INSTALL.md)** para obter orientações completas de instalação e recuperação.

</details>

## Comunidade e suporte

- Leia a [documentação](https://clawbrowser.ai/docs/) e as [perguntas frequentes](https://clawbrowser.ai/faq/).
- Relate problemas reproduzíveis no [GitHub Issues](https://github.com/clawbrowser/clawbrowser/issues).
- Entre no [Discord do Clawbrowser](https://discord.gg/CK62brtKhe) para participar da comunidade.
- Consulte as versões do produto em [`clawbrowser/clawbrowser`](https://github.com/clawbrowser/clawbrowser/releases/latest) e as versões do bootstrapper em [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest).

## Licença

Distribuído sob a licença **MIT**. Texto completo: [opensource.org/licenses/MIT](https://opensource.org/licenses/MIT).
