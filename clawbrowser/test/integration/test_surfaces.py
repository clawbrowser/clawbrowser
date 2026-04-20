"""Integration tests: verify fingerprint surfaces via CDP."""

import json
import re

import pytest


def _expected_ua_full_version(user_agent: str) -> str:
    match = re.search(r"Clawbrowser/([0-9.]+)", user_agent)
    assert match, f"Could not parse Clawbrowser version from {user_agent!r}"
    return match.group(1)


def _expected_ua_major_version(user_agent: str) -> str:
    return _expected_ua_full_version(user_agent).split(".")[0]


async def _echo_request_headers(page):
    payload = await page.evaluate("""() => fetch('/__headers').then(r => {
        if (!r.ok) {
            throw new Error(`unexpected status ${r.status}`);
        }
        return r.json();
    })""")
    return {key.lower(): value for key, value in payload["headers"].items()}


async def _timezones_match(page, expected_timezone: str) -> bool:
    return await page.evaluate(
        """expectedTimezone => {
            const canonicalize = value => {
                try {
                    return new Intl.DateTimeFormat('en-US', {
                        timeZone: value
                    }).resolvedOptions().timeZone;
                } catch (error) {
                    return value;
                }
            };

            const actualTimezone =
                Intl.DateTimeFormat().resolvedOptions().timeZone;
            return canonicalize(expectedTimezone) ===
                canonicalize(actualTimezone);
        }""",
        expected_timezone,
    )


@pytest.mark.asyncio
async def test_navigator_user_agent(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    actual = await page.evaluate("navigator.userAgent")
    assert actual == fp["user_agent"]


@pytest.mark.asyncio
async def test_navigator_platform(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    actual = await page.evaluate("navigator.platform")
    assert actual == fp["platform"]


@pytest.mark.asyncio
async def test_network_user_agent_header(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]

    headers = await _echo_request_headers(page)
    assert headers["user-agent"] == fp["user_agent"]


@pytest.mark.asyncio
async def test_navigator_user_agent_data(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    expected_full_version = _expected_ua_full_version(fp["user_agent"])

    actual = await page.evaluate("""async () => {
        if (!navigator.userAgentData) {
            return null;
        }
        const highEntropy = await navigator.userAgentData.getHighEntropyValues([
            'fullVersionList'
        ]);
        return {
            mobile: navigator.userAgentData.mobile,
            platform: navigator.userAgentData.platform,
            brands: navigator.userAgentData.brands,
            fullVersionList: highEntropy.fullVersionList || []
        };
    }""")

    assert actual is not None
    assert actual["mobile"] is False
    assert actual["platform"] == "macOS"
    assert any(
        brand["version"] == expected_full_version
        for brand in actual["fullVersionList"]
    ), actual


@pytest.mark.asyncio
async def test_sec_ch_ua_headers(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    expected_major_version = _expected_ua_major_version(fp["user_agent"])

    headers = await _echo_request_headers(page)
    assert headers["sec-ch-ua-mobile"] == "?0"
    assert headers["sec-ch-ua-platform"] == '"macOS"'
    assert expected_major_version in headers["sec-ch-ua"]


@pytest.mark.asyncio
async def test_navigator_language(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    actual = await page.evaluate("navigator.language")
    assert actual == fp["language"][0]


@pytest.mark.asyncio
async def test_navigator_languages(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    actual = await page.evaluate("JSON.stringify(navigator.languages)")
    assert json.loads(actual) == fp["language"]


@pytest.mark.asyncio
async def test_navigator_hardware_concurrency(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    actual = await page.evaluate("navigator.hardwareConcurrency")
    assert actual == fp["hardware"]["concurrency"]


@pytest.mark.asyncio
async def test_navigator_device_memory(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    actual = await page.evaluate("navigator.deviceMemory")
    assert actual == fp["hardware"]["memory"]


@pytest.mark.asyncio
async def test_screen_dimensions(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    assert await page.evaluate("screen.width") == fp["screen"]["width"]
    assert await page.evaluate("screen.height") == fp["screen"]["height"]
    assert await page.evaluate("screen.availWidth") == fp["screen"]["avail_width"]
    assert await page.evaluate("screen.availHeight") == fp["screen"]["avail_height"]
    assert await page.evaluate("screen.colorDepth") == fp["screen"]["color_depth"]


@pytest.mark.asyncio
async def test_device_pixel_ratio(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    actual = await page.evaluate("window.devicePixelRatio")
    assert actual == fp["screen"]["pixel_ratio"]


@pytest.mark.asyncio
async def test_timezone(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    assert await _timezones_match(page, fp["timezone"])


@pytest.mark.asyncio
async def test_webgl_vendor_renderer(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    result = await page.evaluate("""() => {
        const canvas = document.createElement('canvas');
        const gl = canvas.getContext('webgl');
        if (!gl) return null;
        const ext = gl.getExtension('WEBGL_debug_renderer_info');
        if (!ext) return null;
        return {
            vendor: gl.getParameter(ext.UNMASKED_VENDOR_WEBGL),
            renderer: gl.getParameter(ext.UNMASKED_RENDERER_WEBGL)
        };
    }""")
    assert result is not None
    assert result["vendor"] == fp["webgl"]["vendor"]
    assert result["renderer"] == fp["webgl"]["renderer"]


@pytest.mark.asyncio
async def test_canvas_determinism(browser_with_fingerprint):
    page, _ = browser_with_fingerprint
    script = """async () => {
        function draw() {
            const c = document.createElement('canvas');
            c.width = 200; c.height = 50;
            const ctx = c.getContext('2d');
            ctx.fillStyle = '#f60';
            ctx.fillRect(125, 1, 62, 20);
            ctx.fillStyle = '#069';
            ctx.font = '14px Arial';
            ctx.fillText('test', 2, 15);
            return c.toDataURL();
        }
        return draw() === draw();
    }"""
    assert await page.evaluate(script) is True


@pytest.mark.asyncio
async def test_audio_determinism(browser_with_fingerprint):
    page, _ = browser_with_fingerprint
    script = """async () => {
        async function audioHash() {
            const ctx = new OfflineAudioContext(1, 44100, 44100);
            const osc = ctx.createOscillator();
            osc.type = 'triangle';
            osc.frequency.setValueAtTime(10000, ctx.currentTime);
            osc.connect(ctx.destination);
            osc.start(0);
            const buf = await ctx.startRendering();
            const data = buf.getChannelData(0);
            let hash = 0;
            for (let i = 0; i < data.length; i++) {
                hash = ((hash << 5) - hash + Math.round(data[i] * 1e10)) | 0;
            }
            return hash;
        }
        const h1 = await audioHash();
        const h2 = await audioHash();
        return h1 === h2;
    }"""
    assert await page.evaluate(script) is True


@pytest.mark.asyncio
async def test_client_rects_stability(browser_with_fingerprint):
    page, _ = browser_with_fingerprint
    script = """() => {
        const el = document.createElement('div');
        el.style.cssText = 'position:absolute;top:10px;left:10px;width:100px;height:50px;';
        document.body.appendChild(el);
        const r1 = el.getBoundingClientRect();
        const r2 = el.getBoundingClientRect();
        document.body.removeChild(el);
        return r1.x === r2.x && r1.y === r2.y &&
               r1.width === r2.width && r1.height === r2.height;
    }"""
    assert await page.evaluate(script) is True


@pytest.mark.asyncio
async def test_plugins_count(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    if "plugins" in fp:
        actual = await page.evaluate("navigator.plugins.length")
        assert actual == len(fp["plugins"])


@pytest.mark.asyncio
async def test_battery(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    if "battery" in fp:
        result = await page.evaluate("""async () => {
            const b = await navigator.getBattery();
            return { charging: b.charging, level: b.level };
        }""")
        assert result["charging"] == fp["battery"]["charging"]
        assert result["level"] == fp["battery"]["level"]


@pytest.mark.asyncio
async def test_media_devices_count(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    if "media_devices" in fp:
        actual = await page.evaluate(
            "navigator.mediaDevices.enumerateDevices().then(d => d.length)"
        )
        assert actual == len(fp["media_devices"])


@pytest.mark.asyncio
async def test_speech_synthesis(browser_with_fingerprint):
    """speechSynthesis.getVoices() should match fingerprint speech_voices list."""
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    if "speech_voices" not in fp:
        pytest.skip("No speech_voices in test fixture")

    voices = await page.evaluate("""() => new Promise((resolve) => {
        const voices = speechSynthesis.getVoices();
        if (voices.length > 0) {
            resolve(voices.map(v => v.name));
        } else {
            speechSynthesis.onvoiceschanged = () => {
                resolve(speechSynthesis.getVoices().map(v => v.name));
            };
            setTimeout(() => resolve(speechSynthesis.getVoices().map(v => v.name)), 3000);
        }
    })""")
    expected_voices = fp["speech_voices"]
    assert sorted(voices) == sorted(expected_voices), (
        f"Expected {expected_voices}, got {voices}"
    )


@pytest.mark.asyncio
async def test_fonts_detect_all_expected(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]

    detected = await page.evaluate(
        """fonts => {
        return fonts.filter(font => {
            const measureWidth = fontFamily => {
                const span = document.createElement('span');
                span.style.fontFamily = fontFamily;
                span.textContent = 'mmmmmmmmmmlli';
                document.body.appendChild(span);
                const width = span.offsetWidth;
                document.body.removeChild(span);
                return width;
            };

            return measureWidth(`"${font}", monospace`) !==
                measureWidth('monospace');
        });
    }""",
        fp["fonts"],
    )

    assert detected == fp["fonts"]


@pytest.mark.asyncio
async def test_verify_page_waits_for_speech_result(
    verify_browser_with_fingerprint,
):
    """Verify page should not finalize before speech voices are reported."""
    page, data = verify_browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    if "speech_voices" not in fp:
        pytest.skip("No speech_voices in test fixture")

    await page.wait_for_function(
        """() => {
        const result = window.__clawbrowser_verify;
        if (!result) {
            return false;
        }
        if (result.status !== 'pass' && result.status !== 'fail') {
            return false;
        }
        return (result.checks || []).some(
            check => check.surface === 'speechSynthesis.voices'
        );
    }"""
    )

    result = await page.evaluate("window.__clawbrowser_verify")
    assert any(
        check["surface"] == "speechSynthesis.voices"
        for check in result["checks"]
    ), f"Missing speech verification result: {result['checks']}"


@pytest.mark.asyncio
async def test_verify_page_fonts_require_all_expected(
    verify_browser_with_fingerprint,
):
    page, data = verify_browser_with_fingerprint
    fp = data["response"]["fingerprint"]

    await page.wait_for_function(
        """() => {
        const result = window.__clawbrowser_verify;
        if (!result) {
            return false;
        }
        return (result.checks || []).some(check => check.surface === 'fonts');
    }"""
    )

    result = await page.evaluate("window.__clawbrowser_verify")
    font_check = next(
        check for check in result["checks"] if check["surface"] == "fonts"
    )
    assert font_check["pass"] is True
    assert font_check["detail"] == "0 missing"
    assert json.loads(font_check["expected"]) == fp["fonts"]
    assert json.loads(font_check["actual"]) == fp["fonts"]
