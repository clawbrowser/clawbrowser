<p align="center">
  <img src="../../../assets/side-bite.svg" alt="Clawbrowser" width="104" />
</p>

<h1 align="center">Clawbrowser</h1>

<p align="center">
  <strong>Управляемая среда выполнения Chromium для ИИ-агентов.</strong><br />
  Храните профили браузера, цифровые отпечатки, маршрутизацию через прокси, cookies и хранилище вместе за стандартной конечной точкой CDP.
</p>

<p align="center">
  <a href="https://clawbrowser.ai/">Сайт</a> ·
  <a href="https://clawbrowser.ai/docs/">Документация</a> ·
  <a href="https://app.clawbrowser.ai/">Получить API-ключ</a> ·
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest">Релизы</a> ·
  <a href="https://discord.gg/CK62brtKhe">Discord</a>
</p>

<p align="center">
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest"><img src="https://img.shields.io/github/v/release/clawbrowser/clawbrowser?label=release&color=2ea44f" alt="Последний релиз" /></a>
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/license-MIT-2ea44f" alt="Лицензия MIT" /></a>
  <img src="https://img.shields.io/badge/platforms-macOS%20%7C%20Linux%20%7C%20Windows-0969da" alt="macOS, Linux и Windows" />
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
  <img src="../../../assets/clawbrowser-site-demo.gif" alt="Предпросмотр сайта и рабочего процесса Clawbrowser" width="960" />
</p>

## Почему Clawbrowser

Автоматизация браузера часто нарушается, когда сигналы браузера, сети, локали и сессии не согласованы. Clawbrowser управляет этим уровнем идентичности внутри среды выполнения Chromium и предоставляет доступ к запущенному браузеру через стандартный Chrome DevTools Protocol (CDP).

- Цифровой отпечаток, привязка к прокси, cookies и хранилище каждого именованного профиля остаются изолированными.
- Профиль можно использовать повторно, когда агенту важно сохранять непрерывность между запусками.
- Подключайте Playwright, Puppeteer или другой CDP-клиент к конечной точке, которую возвращает `clawctl`.
- Проверяйте активный профиль с помощью встроенной страницы `clawbrowser://verify/`.

Clawbrowser предназначен для сокращения сбоев, вызванных несогласованными сигналами идентичности браузера. Он не является универсальным средством обхода CAPTCHA и не гарантирует доступ к любому сайту.

## Быстрый старт

Получите API-ключ на **[app.clawbrowser.ai](https://app.clawbrowser.ai/)**, затем выполните:

```bash
clawctl install --json
clawctl config set --api-key "$CLAWBROWSER_API_KEY"
clawctl start --profile work --url https://example.com --json
clawctl endpoint --profile work --json
# Connect the returned CDP endpoint with Playwright, Puppeteer, or your agent.
```

Повторно запрашивайте конечную точку с помощью `clawctl endpoint` после запуска, перезапуска или сбоя: конечные точки CDP временные, поэтому их не следует хранить в конфигурации.

<details>
<summary><b>Поручить установку ИИ-агенту для программирования</b></summary>

Вставьте следующий промпт в Claude Code, Codex, Cursor, Gemini CLI или другой агент для программирования:

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

## Установка

`clawctl` — поддерживаемый загрузчик. Начните с отдельного архива для ОС и архитектуры хоста из [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest). Затем выполните `clawctl install`: команда установит Clawbrowser или использует уже установленную версию, при необходимости добавит переносимую среду выполнения Linux и настроит поддерживаемые интеграции с агентами.

Не используйте в качестве способа первоначальной установки `npm`, `npx`, передаваемый через curl установщик, Docker, архив полезной нагрузки браузера или необработанную копию исходного кода.

> [!IMPORTANT]
> Не распаковывайте `clawctl` в `/tmp`. Во многих контейнерах для агентов `/tmp` подключается с параметром `noexec`; вместо него используйте постоянный рабочий каталог, в котором разрешено выполнение файлов.

<details open>
<summary><b>Linux</b> (x64 или ARM64; сервер, контейнер или хост без дисплея)</summary>

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

Переносимый процесс установки для Linux поддерживает хосты glibc `amd64` и `arm64` и не требует Docker, `sudo`, `apt`, физического дисплея или ручной загрузки среды выполнения.

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

В macOS используется `Clawbrowser.app`, а для работы требуется активная пользовательская сессия с графическим рабочим столом.

</details>

<details>
<summary><b>Windows</b> (64-разрядная PowerShell)</summary>

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

Если полезная нагрузка браузера содержит `setup.exe`, команда `clawctl install` запускает его в тихом режиме, а Windows может запросить разрешение администратора.

</details>

Сведения о постоянных рабочих каталогах, точных ролях архивов, автономной установке, инфраструктуре на основе Docker и устранении неполадок приведены в **[INSTALL.md](../../../INSTALL.md)**.

## Основные возможности

| Возможность | Что она предоставляет |
| --- | --- |
| Управляемая идентичность | Сгенерированный профиль сохраняет связанные поверхности цифрового отпечатка согласованными внутри среды выполнения браузера. |
| Профили, привязанные к прокси | Маршрутизация через residential- или datacenter-прокси может быть связана со сгенерированным профилем. |
| Изолированные сессии | Именованные профили хранят собственные cookies, данные, идентичность и конечную точку. |
| Стандартный доступ по CDP | Playwright, Puppeteer и другие CDP-клиенты подключаются к активной конечной точке браузера. |
| Удалённый просмотр | `clawctl remote` может вернуть временный `dashboard_url` для наблюдения за запущенным профилем или управления им. |
| Интеграции с агентами | `clawctl install` записывает поддерживаемые шаблоны интеграции в каталоги, которые используют выбранные агенты. |

## Как это работает

1. **Установите среду выполнения.** `clawctl install` устанавливает Clawbrowser или использует уже установленную версию и подготавливает необходимую переносимую среду выполнения.
2. **Сохраните данные аутентификации.** `clawctl config set --api-key …` записывает API-ключ в каталог конфигурации Clawbrowser.
3. **Запустите профиль.** `clawctl start --profile <name>` запускает управляемый Chromium и ожидает его конечную точку CDP.
4. **Проверьте идентичность.** `clawbrowser://verify/` показывает исходящий прокси-адрес и сгенерированные поверхности цифрового отпечатка.
5. **Подключите агента.** Получите текущую конечную точку с помощью `clawctl endpoint`, а затем подключитесь через стандартный CDP-клиент.

## Подключение CDP-клиента

Используйте конечную точку, которую возвращает `clawctl endpoint --profile work --json`.

<details open>
<summary><b>Playwright для Python</b></summary>

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
<summary><b>Playwright для Node.js</b></summary>

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
> Не переопределяйте свойства цифрового отпечатка через CDP. Clawbrowser применяет их внутри движка, а клиентские переопределения могут создать противоречивые сигналы.

## Распространённые рабочие процессы

<details>
<summary><b>CLI, удалённый просмотр и несколько профилей</b></summary>

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

Считайте возвращённый `dashboard_url` конфиденциальной временной ссылкой для управления. При повторном использовании имени профиля повторно используются его кэшированная идентичность и данные сессии.

</details>

## Поддерживаемые платформы

| Платформа | Режим среды выполнения | Примечания |
| --- | --- | --- |
| macOS | Нативное настольное приложение | Apple Silicon; требуется активная пользовательская сессия с графическим рабочим столом. |
| Linux | Переносимая среда выполнения | glibc `amd64` и `arm64`; подходит для серверов, контейнеров и хостов без дисплея. |
| Windows | Нативная установка | 64-разрядная Windows через PowerShell; установка может вызвать запрос UAC. |

## Сценарии использования

- Исследования и сбор структурированных данных с помощью ИИ.
- Проверка качества сайтов посредством реальных пользовательских сценариев в браузере.
- Повторяющиеся процессы мониторинга, которым требуется непрерывность сессии.
- Работа с несколькими аккаунтами через изолированные профили браузера.

Используйте Clawbrowser только для тех систем и данных, к которым у вас есть разрешённый доступ, и соблюдайте условия целевого сайта и применимое законодательство.

## Устранение неполадок

<details>
<summary><b>Распространённые проблемы установки и сессии</b></summary>

| Симптом | Действие |
| --- | --- |
| `clawctl: command not found` | Запустите бинарный файл из распакованного отдельного архива или добавьте его каталог в `PATH`. |
| `Permission denied` после `chmod +x` | Распакуйте архив заново вне `/tmp` или другой файловой системы с параметром `noexec`. |
| API-ключ запрашивается повторно | Сохраните его один раз с помощью `clawctl config set --api-key "$CLAWBROWSER_API_KEY"`. |
| Конечная точка CDP отклоняет подключение или устарела | После запуска или перезапуска профиля снова выполните `clawctl endpoint --profile <name> --json`. |
| Время запуска браузера истекает | Повторите попытку, проверьте журналы запуска, доступное дисковое пространство и сетевое подключение. |

Полное руководство по установке и восстановлению приведено в **[INSTALL.md](../../../INSTALL.md)**.

</details>

## Сообщество и поддержка

- Ознакомьтесь с [документацией](https://clawbrowser.ai/docs/) и [FAQ](https://clawbrowser.ai/faq/).
- Сообщайте о воспроизводимых проблемах в [GitHub Issues](https://github.com/clawbrowser/clawbrowser/issues).
- Присоединяйтесь к [Discord-сообществу Clawbrowser](https://discord.gg/CK62brtKhe).
- Следите за релизами продукта в [`clawbrowser/clawbrowser`](https://github.com/clawbrowser/clawbrowser/releases/latest), а за релизами загрузчика — в [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest).

## Лицензия

Распространяется по лицензии **MIT**. Полный текст: [opensource.org/licenses/MIT](https://opensource.org/licenses/MIT).
