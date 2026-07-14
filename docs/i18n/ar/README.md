<p align="center">
  <img src="../../../assets/side-bite.svg" alt="Clawbrowser" width="104" />
</p>

<h1 align="center">Clawbrowser</h1>

<p align="center">
  <strong>بيئة تشغيل Chromium مُدارة لوكلاء الذكاء الاصطناعي.</strong><br />
  احتفظ بملفات تعريف المتصفح والبصمات وتوجيه الوكيل وملفات تعريف الارتباط والتخزين معًا خلف نقطة نهاية CDP قياسية.
</p>

<p align="center">
  <a href="https://clawbrowser.ai/">الموقع</a> ·
  <a href="https://clawbrowser.ai/docs/">الوثائق</a> ·
  <a href="https://app.clawbrowser.ai/">الحصول على مفتاح API</a> ·
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest">الإصدارات</a> ·
  <a href="https://discord.gg/CK62brtKhe">Discord</a>
</p>

<p align="center">
  <a href="https://github.com/clawbrowser/clawbrowser/releases/latest"><img src="https://img.shields.io/github/v/release/clawbrowser/clawbrowser?label=release&color=2ea44f" alt="أحدث إصدار" /></a>
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/license-MIT-2ea44f" alt="ترخيص MIT" /></a>
  <img src="https://img.shields.io/badge/platforms-macOS%20%7C%20Linux%20%7C%20Windows-0969da" alt="macOS وLinux وWindows" />
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
  <img src="../../../assets/clawbrowser-site-demo.gif" alt="معاينة موقع Clawbrowser وسير العمل" width="960" />
</p>

## لماذا Clawbrowser

غالبًا ما تتعطل أتمتة المتصفح عندما لا تتوافق إشارات المتصفح والشبكة والإعدادات المحلية والجلسة. يدير Clawbrowser طبقة الهوية هذه داخل بيئة تشغيل Chromium ويتيح المتصفح الجاري عبر Chrome DevTools Protocol (CDP) القياسي.

- حافظ على عزل بصمة كل ملف تعريف مُسمّى وربطه بالوكيل وملفات تعريف الارتباط والتخزين الخاص به.
- أعد استخدام ملف تعريف عندما يحتاج الوكيل إلى الاستمرارية بين مرات التشغيل.
- صِل Playwright أو Puppeteer أو أي عميل CDP آخر بنقطة النهاية التي يعيدها `clawctl`.
- افحص ملف التعريف النشط باستخدام صفحة `clawbrowser://verify/` المضمّنة.

صُمم Clawbrowser للحد من الانقطاعات الناتجة عن عدم اتساق إشارات هوية المتصفح. وهو ليس وسيلة عامة لتجاوز CAPTCHA ولا يضمن الوصول إلى كل موقع إلكتروني.

## البدء السريع

احصل على مفتاح API من **[app.clawbrowser.ai](https://app.clawbrowser.ai/)**، ثم شغّل:

```bash
clawctl install --json
clawctl config set --api-key "$CLAWBROWSER_API_KEY"
clawctl start --profile work --url https://example.com --json
clawctl endpoint --profile work --json
# Connect the returned CDP endpoint with Playwright, Puppeteer, or your agent.
```

اطلب نقطة النهاية من جديد باستخدام `clawctl endpoint` بعد بدء التشغيل أو إعادة التشغيل أو حدوث فشل؛ نقاط نهاية CDP مؤقتة ويجب ألا تُحفظ في الإعدادات.

<details>
<summary><b>السماح لوكيل برمجة بالذكاء الاصطناعي بتنفيذ التثبيت</b></summary>

ألصق الموجّه التالي في Claude Code أو Codex أو Cursor أو Gemini CLI أو وكيل برمجة آخر:

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

## التثبيت

`clawctl` هو برنامج التمهيد المدعوم. ابدأ بالأرشيف المستقل المتوافق مع نظام تشغيل المضيف ومعماريته من [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest). بعد ذلك شغّل `clawctl install` ليتمكن من تثبيت Clawbrowser أو إعادة استخدامه، وإضافة بيئة تشغيل Linux المحمولة عند الحاجة، وتهيئة تكاملات الوكلاء المدعومة.

لا تبدأ التثبيت باستخدام `npm` أو `npx` أو مُثبّت يُمرر عبر curl أو Docker أو أرشيف حمولة المتصفح أو نسخة خام من الشفرة المصدرية.

> [!IMPORTANT]
> لا تفك ضغط `clawctl` داخل `/tmp`. تُركّب حاويات وكلاء كثيرة `/tmp` بخيار `noexec`؛ استخدم بدلًا منه دليل عمل دائمًا يسمح بالتنفيذ.

<details open>
<summary><b>Linux</b> (‏x64 أو ARM64؛ خادم أو حاوية أو مضيف بلا شاشة)</summary>

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

يدعم مسار Linux المحمول مضيفات glibc بمعماريتي `amd64` و`arm64` ولا يتطلب Docker أو `sudo` أو `apt` أو شاشة فعلية أو تنزيل بيئة التشغيل يدويًا.

</details>

<details>
<summary><b>macOS</b> (‏Apple Silicon)</summary>

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

يستخدم macOS التطبيق `Clawbrowser.app` ويتطلب سياق سطح مكتب رسوميًا لمستخدم مسجّل الدخول.

</details>

<details>
<summary><b>Windows</b> (‏PowerShell إصدار 64 بت)</summary>

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

إذا كانت حمولة المتصفح تحتوي على `setup.exe`، فسيشغّله `clawctl install` بصمت وقد يطلب Windows موافقة المسؤول.

</details>

للتعرّف على أدلة العمل الدائمة والأدوار الدقيقة للأرشيفات وعمليات الإعداد بلا اتصال والبنية التحتية المدعومة بـ Docker واستكشاف الأخطاء، اقرأ **[INSTALL.md](../../../INSTALL.md)**.

## الإمكانات الأساسية

| الإمكانية | ما الذي توفره |
| --- | --- |
| هوية مُدارة | يحافظ ملف التعريف المُنشأ على ترابط أسطح البصمة ذات الصلة داخل بيئة تشغيل المتصفح. |
| ملفات تعريف مرتبطة بالوكيل | يمكن ربط توجيه residential أو datacenter بملف التعريف المُنشأ. |
| جلسات معزولة | يحتفظ كل ملف تعريف مُسمّى بملفات تعريف الارتباط والتخزين والهوية ونقطة النهاية الخاصة به. |
| وصول CDP قياسي | يتصل Playwright وPuppeteer وعملاء CDP الآخرون بنقطة نهاية المتصفح النشط. |
| عرض عن بُعد | يمكن لـ `clawctl remote` إعادة `dashboard_url` مؤقت لمشاهدة ملف تعريف قيد التشغيل أو التحكم فيه. |
| تكاملات الوكلاء | يكتب `clawctl install` قوالب التكامل المدعومة في المواقع التي تستخدمها الوكلاء المحددة. |

## آلية العمل

1. **ثبّت بيئة التشغيل.** يثبّت `clawctl install` تطبيق Clawbrowser أو يعيد استخدامه ويجهّز أي بيئة تشغيل محمولة مطلوبة.
2. **احفظ بيانات المصادقة.** يكتب `clawctl config set --api-key …` مفتاح API في دليل إعدادات Clawbrowser.
3. **ابدأ ملف تعريف.** يشغّل `clawctl start --profile <name>` متصفح Chromium المُدار وينتظر نقطة نهاية CDP الخاصة به.
4. **تحقق من الهوية.** تعرض `clawbrowser://verify/` مسار خروج الوكيل وأسطح البصمة المُنشأة.
5. **صِل الوكيل.** اقرأ نقطة النهاية الحالية باستخدام `clawctl endpoint`، ثم اتصل من خلال عميل CDP قياسي.

## توصيل عميل CDP

استخدم نقطة النهاية التي يعيدها `clawctl endpoint --profile work --json`.

<details open>
<summary><b>Playwright مع Python</b></summary>

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
<summary><b>Playwright مع Node.js</b></summary>

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
> لا تتجاوز خصائص البصمة عبر CDP. يطبقها Clawbrowser داخل المحرك؛ وقد تنشئ التجاوزات من جهة العميل إشارات متعارضة.

## مسارات العمل الشائعة

<details>
<summary><b>واجهة CLI والعرض عن بُعد وملفات التعريف المتعددة</b></summary>

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

تعامل مع `dashboard_url` المُعاد بوصفه رابط تحكم مؤقتًا وحساسًا. تؤدي إعادة استخدام اسم ملف التعريف إلى إعادة استخدام هويته وبيانات جلسته المخزنة مؤقتًا.

</details>

## دعم المنصات

| المنصة | وضع بيئة التشغيل | ملاحظات |
| --- | --- | --- |
| macOS | تطبيق سطح مكتب أصلي | ‏Apple Silicon؛ يتطلب سطح مكتب رسوميًا لمستخدم مسجّل الدخول. |
| Linux | بيئة تشغيل محمولة | ‏glibc بمعماريتي `amd64` و`arm64`؛ مناسبة للخوادم والحاويات والمضيفين بلا شاشة. |
| Windows | تثبيت أصلي | ‏Windows إصدار 64 بت عبر PowerShell؛ قد يؤدي التثبيت إلى ظهور مطالبة UAC. |

## حالات الاستخدام

- البحث بمساعدة الذكاء الاصطناعي وجمع البيانات المنظمة.
- ضمان جودة المواقع من خلال رحلات حقيقية في المتصفح.
- مسارات المراقبة المتكررة التي تحتاج إلى استمرارية الجلسة.
- عمليات الحسابات متعددة الملفات مع حالة متصفح معزولة.

استخدم Clawbrowser فقط على الأنظمة والبيانات المصرح لك بالوصول إليها، والتزم بشروط الموقع المستهدف والقوانين المعمول بها.

## استكشاف الأخطاء وإصلاحها

<details>
<summary><b>مشكلات التثبيت والجلسات الشائعة</b></summary>

| العَرَض | الإجراء |
| --- | --- |
| `clawctl: command not found` | شغّل الملف التنفيذي من الأرشيف المستقل بعد فك ضغطه أو أضف دليله إلى `PATH`. |
| `Permission denied` بعد `chmod +x` | أعد فك الضغط خارج `/tmp` أو أي نظام ملفات آخر بخيار `noexec`. |
| يُطلب مفتاح API مرارًا | احفظه مرة واحدة باستخدام `clawctl config set --api-key "$CLAWBROWSER_API_KEY"`. |
| نقطة نهاية CDP ترفض الاتصال أو أصبحت قديمة | شغّل `clawctl endpoint --profile <name> --json` مرة أخرى بعد بدء ملف التعريف أو إعادة تشغيله. |
| انتهاء مهلة بدء المتصفح | أعد المحاولة وافحص سجلات بدء التشغيل وتحقق من مساحة القرص المتاحة والوصول إلى الشبكة. |

راجع **[INSTALL.md](../../../INSTALL.md)** للحصول على إرشادات التثبيت والاسترداد الكاملة.

</details>

## المجتمع والدعم

- اقرأ [الوثائق](https://clawbrowser.ai/docs/) و[الأسئلة الشائعة](https://clawbrowser.ai/faq/).
- أبلغ عن المشكلات القابلة لإعادة الإنتاج في [GitHub Issues](https://github.com/clawbrowser/clawbrowser/issues).
- انضم إلى [Discord الخاص بـ Clawbrowser](https://discord.gg/CK62brtKhe) لمناقشات المجتمع.
- راجع إصدارات المنتج في [`clawbrowser/clawbrowser`](https://github.com/clawbrowser/clawbrowser/releases/latest) وإصدارات برنامج التمهيد في [`clawbrowser/clawctl`](https://github.com/clawbrowser/clawctl/releases/latest).

## الترخيص

يُوزع بموجب ترخيص **MIT**. النص الكامل: [opensource.org/licenses/MIT](https://opensource.org/licenses/MIT).
