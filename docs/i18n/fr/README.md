<p align="center">
  <img src="../../../assets/side-bite.svg" alt="Clawbrowser" width="104" />
</p>

<h1 align="center">Clawbrowser</h1>

<p align="center">
  <strong>L'environnement d'exécution Chromium géré pour les agents d'IA.</strong><br />
  Regroupez les profils de navigateur, les empreintes numériques, le routage proxy, les cookies et le stockage derrière un endpoint CDP standard.
</p>

<p align="center">
  <a href="https://clawbrowser.ai/">Site web</a> ·
  <a href="https://clawbrowser.ai/docs/">Documentation</a> ·
  <a href="https://app.clawbrowser.ai/">Obtenir une clé API</a> ·
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest">Versions</a> ·
  <a href="https://discord.gg/DWuwhYZVn">Discord</a>
</p>

<p align="center">
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest"><img src="https://img.shields.io/github/v/release/clawbrowser/clawbrowser?label=release&color=2ea44f" alt="Dernière version" /></a>
  <a href="../../../LICENSE"><img src="https://img.shields.io/badge/license-MIT-2ea44f" alt="Licence MIT" /></a>
  <img src="https://img.shields.io/badge/platforms-macOS%20%7C%20Linux%20%7C%20Windows-0969da" alt="macOS, Linux et Windows" />
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
  <img src="../../../assets/clawbrowser-site-demo.gif" alt="Aperçu du site web et du flux de travail de Clawbrowser" width="960" />
</p>

## Pourquoi Clawbrowser

L'automatisation du navigateur échoue souvent lorsque les signaux du navigateur, du réseau, des paramètres régionaux et de la session ne concordent pas. Clawbrowser gère cette couche d'identité dans un environnement d'exécution Chromium et expose le navigateur actif via le standard Chrome DevTools Protocol (CDP).

- Isolez l'empreinte numérique, la liaison au proxy, les cookies et le stockage de chaque profil nommé.
- Réutilisez un profil lorsqu'un agent a besoin de continuité entre les exécutions.
- Connectez Playwright, Puppeteer ou un autre client CDP à l'endpoint renvoyé par `clawctl`.
- Inspectez le profil actif avec la page intégrée `clawbrowser://verify/`.

Clawbrowser est conçu pour réduire les interruptions causées par des signaux d'identité de navigateur incohérents. Il ne constitue pas un moyen universel de contourner les CAPTCHA et ne garantit pas l'accès à tous les sites web.

## Démarrage rapide

Obtenez une clé API sur **[app.clawbrowser.ai](https://app.clawbrowser.ai/)**, puis exécutez :

```bash
clawctl install --json
clawctl config set --api-key "$CLAWBROWSER_API_KEY"
clawctl start --profile work --url https://example.com --json
clawctl endpoint --profile work --json
# Connect the returned CDP endpoint with Playwright, Puppeteer, or your agent.
```

Récupérez à nouveau l'endpoint avec `clawctl endpoint` après un démarrage, un redémarrage ou un échec ; les endpoints CDP sont temporaires et ne doivent pas être enregistrés dans la configuration.

<details>
<summary><b>Laisser un agent de programmation IA effectuer l'installation</b></summary>

Collez le prompt suivant dans Claude Code, Codex, Cursor, Gemini CLI ou un autre agent de programmation :

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

`clawctl` est le programme d'amorçage pris en charge. Commencez par l'archive autonome correspondant au système d'exploitation et à l'architecture de l'hôte depuis [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest). Exécutez ensuite `clawctl install` afin qu'il puisse installer ou réutiliser Clawbrowser, ajouter l'environnement d'exécution Linux portable si nécessaire et configurer les intégrations d'agents prises en charge.

N'effectuez pas l'amorçage depuis `npm`, `npx`, un programme d'installation transmis à curl, Docker, une archive de charge utile du navigateur ou un checkout brut du code source.

> [!IMPORTANT]
> N'extrayez pas `clawctl` dans `/tmp`. De nombreux conteneurs d'agents montent `/tmp` avec `noexec` ; utilisez plutôt un répertoire de travail exécutable et durable.

<details open>
<summary><b>Linux</b> (x64 ou ARM64 ; serveur, conteneur ou hôte sans affichage)</summary>

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

Le parcours Linux portable prend en charge les hôtes glibc `amd64` et `arm64` et ne nécessite ni Docker, ni `sudo`, ni `apt`, ni écran physique, ni téléchargement manuel de l'environnement d'exécution.

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

macOS utilise `Clawbrowser.app` et nécessite une session ouverte dans un environnement de bureau avec interface graphique.

</details>

<details>
<summary><b>Windows</b> (PowerShell 64 bits)</summary>

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

Si la charge utile du navigateur contient `setup.exe`, `clawctl install` l'exécute silencieusement et Windows peut demander une autorisation d'administrateur.

</details>

Pour les répertoires de travail durables, les rôles exacts des archives, les installations hors ligne, l'infrastructure reposant sur Docker et le dépannage, consultez **[INSTALL.md](../../../INSTALL.md)**.

## Fonctionnalités principales

| Fonctionnalité | Ce qu'elle fournit |
| --- | --- |
| Identité gérée | Un profil généré maintient ensemble les surfaces d'empreinte numérique associées dans l'environnement d'exécution du navigateur. |
| Profils liés à un proxy | Le routage résidentiel ou de centre de données peut être associé au profil généré. |
| Sessions isolées | Les profils nommés conservent leurs propres cookies, stockage, identité et endpoint. |
| Accès CDP standard | Playwright, Puppeteer et d'autres clients CDP se connectent à l'endpoint du navigateur actif. |
| Visualisation à distance | `clawctl remote` peut renvoyer une `dashboard_url` temporaire pour observer ou contrôler un profil actif. |
| Intégrations d'agents | `clawctl install` écrit les modèles d'intégration pris en charge aux emplacements utilisés par les agents sélectionnés. |

## Fonctionnement

1. **Installez l'environnement d'exécution.** `clawctl install` installe ou réutilise Clawbrowser et prépare tout environnement d'exécution portable nécessaire.
2. **Enregistrez l'authentification.** `clawctl config set --api-key …` écrit la clé API dans le répertoire de configuration de Clawbrowser.
3. **Démarrez un profil.** `clawctl start --profile <name>` lance le Chromium géré et attend son endpoint CDP.
4. **Vérifiez l'identité.** `clawbrowser://verify/` indique la sortie proxy et les surfaces d'empreinte numérique générées.
5. **Connectez l'agent.** Lisez l'endpoint actuel avec `clawctl endpoint`, puis connectez-vous à l'aide d'un client CDP standard.

## Connecter un client CDP

Utilisez l'endpoint renvoyé par `clawctl endpoint --profile work --json`.

<details open>
<summary><b>Playwright pour Python</b></summary>

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
<summary><b>Playwright pour Node.js</b></summary>

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
> Ne remplacez pas les propriétés d'empreinte numérique via CDP. Clawbrowser les applique au sein du moteur ; les remplacements côté client peuvent créer des signaux contradictoires.

## Flux de travail courants

<details>
<summary><b>CLI, visualisation à distance et profils multiples</b></summary>

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

Traitez toute `dashboard_url` renvoyée comme un lien de contrôle temporaire sensible. Réutiliser un nom de profil réutilise son identité mise en cache et ses données de session.

</details>

## Plateformes prises en charge

| Plateforme | Mode d'exécution | Remarques |
| --- | --- | --- |
| macOS | Application de bureau native | Apple Silicon ; nécessite une session ouverte dans un environnement de bureau avec interface graphique. |
| Linux | Environnement d'exécution portable | glibc `amd64` et `arm64` ; adapté aux serveurs, conteneurs et hôtes sans affichage. |
| Windows | Installation native | Windows 64 bits via PowerShell ; l'installation peut déclencher l'UAC. |

## Cas d'utilisation

- Recherche assistée par IA et collecte de données structurées.
- Assurance qualité de sites web au moyen de parcours réels dans le navigateur.
- Flux de surveillance répétés qui nécessitent une continuité de session.
- Opérations de comptes multiprofils avec état de navigateur isolé.

N'utilisez Clawbrowser que sur les systèmes et les données auxquels vous êtes autorisé à accéder, et respectez les conditions du site web cible ainsi que la législation applicable.

## Dépannage

<details>
<summary><b>Problèmes courants d'installation et de session</b></summary>

| Symptôme | Action |
| --- | --- |
| `clawctl: command not found` | Exécutez le binaire depuis l'archive autonome extraite ou ajoutez son répertoire au `PATH`. |
| `Permission denied` après `chmod +x` | Extrayez de nouveau l'archive en dehors de `/tmp` ou d'un autre système de fichiers avec `noexec`. |
| La clé API est demandée à plusieurs reprises | Enregistrez-la une fois avec `clawctl config set --api-key "$CLAWBROWSER_API_KEY"`. |
| L'endpoint CDP est refusé ou obsolète | Exécutez de nouveau `clawctl endpoint --profile <name> --json` après avoir démarré ou redémarré le profil. |
| Le démarrage du navigateur expire | Réessayez, examinez les journaux de démarrage et vérifiez l'espace disque disponible ainsi que l'accès au réseau. |

Consultez **[INSTALL.md](../../../INSTALL.md)** pour les instructions complètes d'installation et de récupération.

</details>

## Communauté et assistance

- Consultez la [documentation](https://clawbrowser.ai/docs/) et la [FAQ](https://clawbrowser.ai/faq/).
- Signalez les problèmes reproductibles dans les [GitHub Issues](https://github.com/clawbrowser/clawbrowser/issues).
- Rejoignez le [Discord de Clawbrowser](https://discord.gg/DWuwhYZVn) pour participer aux échanges de la communauté.
- Consultez les versions du produit dans [`clawbrowser/clawbrowser`](https://github.com/clawbrowser/clawbrowser/releases/latest) et les versions du programme d'amorçage dans [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest).

## Licence

Distribué sous licence **MIT**. Texte intégral : [MIT License](../../../LICENSE).
