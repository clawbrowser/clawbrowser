<p align="center">
  <img src="../../../assets/side-bite.svg" alt="Clawbrowser" width="104" />
</p>

<h1 align="center">Clawbrowser</h1>

<p align="center">
  <strong>El entorno de ejecución Chromium administrado para agentes de IA.</strong><br />
  Mantén juntos los perfiles del navegador, las huellas digitales, el enrutamiento de proxy, las cookies y el almacenamiento detrás de un endpoint CDP estándar.
</p>

<p align="center">
  <a href="https://clawbrowser.ai/">Sitio web</a> ·
  <a href="https://clawbrowser.ai/docs/">Documentación</a> ·
  <a href="https://app.clawbrowser.ai/">Obtener una clave API</a> ·
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest">Versiones</a> ·
  <a href="https://discord.gg/CK62brtKhe">Discord</a>
</p>

<p align="center">
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest"><img src="https://img.shields.io/github/v/release/clawbrowser/clawbrowser?label=release&color=2ea44f" alt="Última versión" /></a>
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/license-MIT-2ea44f" alt="Licencia MIT" /></a>
  <img src="https://img.shields.io/badge/platforms-macOS%20%7C%20Linux%20%7C%20Windows-0969da" alt="macOS, Linux y Windows" />
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
  <img src="../../../assets/clawbrowser-site-demo.gif" alt="Vista previa del sitio web y del flujo de trabajo de Clawbrowser" width="960" />
</p>

## Por qué Clawbrowser

La automatización del navegador suele fallar cuando las señales del navegador, la red, la configuración regional y la sesión no concuerdan. Clawbrowser administra esa capa de identidad dentro de un entorno de ejecución Chromium y expone el navegador en ejecución mediante el estándar Chrome DevTools Protocol (CDP).

- Mantén aislados la huella digital, la vinculación de proxy, las cookies y el almacenamiento de cada perfil con nombre.
- Reutiliza un perfil cuando un agente necesite continuidad entre ejecuciones.
- Conecta Playwright, Puppeteer u otro cliente CDP al endpoint devuelto por `clawctl`.
- Inspecciona el perfil activo con la página integrada `clawbrowser://verify/`.

Clawbrowser está diseñado para reducir las interrupciones causadas por señales de identidad del navegador incoherentes. No es una solución universal para eludir CAPTCHA ni garantiza el acceso a todos los sitios web.

## Inicio rápido

Obtén una clave API en **[app.clawbrowser.ai](https://app.clawbrowser.ai/)** y, a continuación, ejecuta:

```bash
clawctl install --json
clawctl config set --api-key "$CLAWBROWSER_API_KEY"
clawctl start --profile work --url https://example.com --json
clawctl endpoint --profile work --json
# Connect the returned CDP endpoint with Playwright, Puppeteer, or your agent.
```

Vuelve a obtener el endpoint con `clawctl endpoint` después de un inicio, reinicio o fallo; los endpoints CDP son temporales y no deben guardarse en la configuración.

<details>
<summary><b>Permitir que un agente de programación con IA realice la instalación</b></summary>

Pega el siguiente prompt en Claude Code, Codex, Cursor, Gemini CLI u otro agente de programación:

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

## Instalación

`clawctl` es el programa de arranque compatible. Empieza con el archivo independiente para el sistema operativo y la arquitectura del host desde [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest). Después, ejecuta `clawctl install` para que pueda instalar o reutilizar Clawbrowser, añadir el entorno de ejecución portátil de Linux cuando sea necesario y configurar las integraciones compatibles con agentes.

No realices el arranque desde `npm`, `npx`, un instalador canalizado mediante curl, Docker, un archivo de carga útil del navegador ni un checkout del código fuente sin procesar.

> [!IMPORTANT]
> No extraigas `clawctl` en `/tmp`. Muchos contenedores de agentes montan `/tmp` con `noexec`; utiliza en su lugar un directorio de trabajo ejecutable y persistente.

<details open>
<summary><b>Linux</b> (x64 o ARM64; servidor, contenedor o host sin pantalla)</summary>

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

El flujo portátil de Linux es compatible con hosts glibc `amd64` y `arm64`, y no requiere Docker, `sudo`, `apt`, una pantalla física ni una descarga manual del entorno de ejecución.

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

macOS utiliza `Clawbrowser.app` y requiere un contexto de escritorio con GUI y una sesión iniciada.

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

Si la carga útil del navegador contiene `setup.exe`, `clawctl install` lo ejecuta de forma silenciosa y Windows puede solicitar la aprobación del administrador.

</details>

Para conocer los directorios de trabajo persistentes, las funciones exactas de los archivos, las instalaciones sin conexión, la infraestructura respaldada por Docker y la resolución de problemas, consulta **[INSTALL.md](../../../INSTALL.md)**.

## Capacidades principales

| Capacidad | Qué proporciona |
| --- | --- |
| Identidad administrada | Un perfil generado mantiene juntas las superficies de huella digital relacionadas dentro del entorno de ejecución del navegador. |
| Perfiles vinculados a proxy | El enrutamiento residencial o de centro de datos puede asociarse con el perfil generado. |
| Sesiones aisladas | Los perfiles con nombre conservan sus propias cookies, almacenamiento, identidad y endpoint. |
| Acceso CDP estándar | Playwright, Puppeteer y otros clientes CDP se conectan al endpoint del navegador activo. |
| Visualización remota | `clawctl remote` puede devolver una `dashboard_url` temporal para observar o controlar un perfil en ejecución. |
| Integraciones con agentes | `clawctl install` escribe las plantillas de integración compatibles en las ubicaciones utilizadas por los agentes seleccionados. |

## Cómo funciona

1. **Instala el entorno de ejecución.** `clawctl install` instala o reutiliza Clawbrowser y prepara cualquier entorno de ejecución portátil necesario.
2. **Guarda la autenticación.** `clawctl config set --api-key …` escribe la clave API en el directorio de configuración de Clawbrowser.
3. **Inicia un perfil.** `clawctl start --profile <name>` inicia Chromium administrado y espera su endpoint CDP.
4. **Verifica la identidad.** `clawbrowser://verify/` informa de la salida del proxy y de las superficies de huella digital generadas.
5. **Conecta el agente.** Lee el endpoint actual con `clawctl endpoint` y, a continuación, conéctate mediante un cliente CDP estándar.

## Conectar un cliente CDP

Utiliza el endpoint devuelto por `clawctl endpoint --profile work --json`.

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
> No sobrescribas las propiedades de huella digital mediante CDP. Clawbrowser las aplica dentro del motor; las sobrescrituras en el lado del cliente pueden crear señales contradictorias.

## Flujos de trabajo habituales

<details>
<summary><b>CLI, visualización remota y varios perfiles</b></summary>

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

Trata la `dashboard_url` devuelta como un enlace temporal de control confidencial. Al reutilizar el nombre de un perfil, se reutilizan su identidad y los datos de sesión almacenados en caché.

</details>

## Compatibilidad con plataformas

| Plataforma | Modo del entorno de ejecución | Notas |
| --- | --- | --- |
| macOS | Aplicación de escritorio nativa | Apple Silicon; requiere un escritorio con GUI y una sesión iniciada. |
| Linux | Entorno de ejecución portátil | glibc `amd64` y `arm64`; adecuado para servidores, contenedores y hosts sin pantalla. |
| Windows | Instalación nativa | Windows de 64 bits mediante PowerShell; la instalación puede activar UAC. |

## Casos de uso

- Investigación asistida por IA y recopilación de datos estructurados.
- Control de calidad de sitios web mediante recorridos reales en el navegador.
- Flujos de supervisión repetidos que necesitan continuidad de sesión.
- Operaciones de cuentas con varios perfiles y estado del navegador aislado.

Utiliza Clawbrowser únicamente en sistemas y datos a los que estés autorizado a acceder, y respeta los términos del sitio web de destino y la legislación aplicable.

## Resolución de problemas

<details>
<summary><b>Problemas habituales de instalación y sesión</b></summary>

| Síntoma | Acción |
| --- | --- |
| `clawctl: command not found` | Ejecuta el binario desde el archivo independiente extraído o añade su directorio a `PATH`. |
| `Permission denied` después de `chmod +x` | Vuelve a extraerlo fuera de `/tmp` o de otro sistema de archivos con `noexec`. |
| Se solicita la clave API repetidamente | Guárdala una vez con `clawctl config set --api-key "$CLAWBROWSER_API_KEY"`. |
| El endpoint CDP se rechaza o está obsoleto | Ejecuta de nuevo `clawctl endpoint --profile <name> --json` después de iniciar o reiniciar el perfil. |
| El inicio del navegador agota el tiempo de espera | Vuelve a intentarlo, revisa los registros de inicio y comprueba el espacio disponible en disco y el acceso a la red. |

Consulta **[INSTALL.md](../../../INSTALL.md)** para obtener las instrucciones completas de instalación y recuperación.

</details>

## Comunidad y soporte

- Consulta la [documentación](https://clawbrowser.ai/docs/) y las [preguntas frecuentes](https://clawbrowser.ai/faq/).
- Informa de problemas reproducibles en [GitHub Issues](https://github.com/clawbrowser/clawbrowser/issues).
- Únete al [Discord de Clawbrowser](https://discord.gg/CK62brtKhe) para participar en la comunidad.
- Consulta las versiones del producto en [`clawbrowser/clawbrowser`](https://github.com/clawbrowser/clawbrowser/releases/latest) y las del programa de arranque en [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest).

## Licencia

Distribuido bajo la licencia **MIT**. Texto completo: [opensource.org/licenses/MIT](https://opensource.org/licenses/MIT).
