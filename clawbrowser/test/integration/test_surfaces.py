"""Integration tests: verify fingerprint surfaces via CDP."""

import asyncio
import json
import sys

import pytest


def _expected_fixture_fonts(fp):
    # The legacy fixture requests fonts absent from the Linux bundle. Startup
    # migrates it to this explicit catalog. Keep this expectation independent
    # of the browser's saved JSON so a wrong migration cannot bless itself.
    if sys.platform == 'linux':
        return ['Arimo', 'Tinos', 'Cousine', 'DejaVu Sans',
                'Noto Sans CJK JP', 'Noto Sans CJK KR', 'Noto Sans CJK SC',
                'Noto Sans CJK TC', 'Noto Sans CJK HK',
                'Lohit Devanagari', 'Noto Sans Thai',
                "Noto Color Emoji", "Noto Sans Bengali", "Noto Sans Ethiopic", "Noto Sans Gujarati", "Noto Sans Gurmukhi", "Noto Sans Kannada", "Noto Sans Khmer", "Noto Sans Malayalam", "Noto Sans Myanmar", "Noto Sans Sinhala", "Noto Sans Tamil", "Noto Sans Telugu"]
    return fp['fonts']


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


def _derived_window_size(screen, seed):
    state = seed or 1

    def next_uint32():
        nonlocal state
        state ^= (state << 13) & ((1 << 64) - 1)
        state ^= state >> 7
        state ^= (state << 17) & ((1 << 64) - 1)
        state &= (1 << 64) - 1
        return state & 0xFFFFFFFF

    available_width = screen["avail_width"] or screen["width"]
    available_height = screen["avail_height"] or screen["height"]
    width = min(max(400, available_width - next_uint32() % 121), available_width)
    height = min(
        max(300, available_height - next_uint32() % 161), available_height
    )
    return {"width": width, "height": height}


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
    expected = fp["user_agent_data"]

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
    assert actual["mobile"] == expected["mobile"]
    assert actual["platform"] == expected["platform"]
    assert actual["brands"] == expected["brands"]
    assert actual["fullVersionList"] == expected["fullVersionList"]


@pytest.mark.asyncio
async def test_sec_ch_ua_headers(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]

    headers = await _echo_request_headers(page)
    assert headers["sec-ch-ua-mobile"] == fp["headers"]["Sec-CH-UA-Mobile"]
    assert headers["sec-ch-ua-platform"] == fp["headers"]["Sec-CH-UA-Platform"]
    assert headers["sec-ch-ua"] == fp["headers"]["Sec-CH-UA"]


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
async def test_navigator_identity_matches_dedicated_worker(browser_with_fingerprint):
    page, _ = browser_with_fingerprint
    actual = await page.evaluate("""async () => {
        const probe = async () => ({
            ua: navigator.userAgent, platform: navigator.platform,
            languages: [...navigator.languages],
            cores: navigator.hardwareConcurrency, memory: navigator.deviceMemory,
            timezone: Intl.DateTimeFormat().resolvedOptions().timeZone,
            offset: new Date().getTimezoneOffset(),
            hints: navigator.userAgentData ? await navigator.userAgentData
                .getHighEntropyValues(['fullVersionList', 'platformVersion',
                                      'architecture', 'bitness']) : null,
        });
        const url = URL.createObjectURL(new Blob([
            `onmessage = async () => postMessage(await (${probe.toString()})())`
        ], {type: 'text/javascript'}));
        const worker = new Worker(url);
        try {
            const result = await new Promise((resolve, reject) => {
                const timer = setTimeout(() => reject(new Error('Worker timed out')), 10000);
                worker.onmessage = event => { clearTimeout(timer); resolve(event.data); };
                worker.onerror = event => { clearTimeout(timer); reject(new Error(event.message)); };
                worker.postMessage(null);
            });
            return {window: await probe(), worker: result};
        } finally {
            worker.terminate();
            URL.revokeObjectURL(url);
        }
    }""")
    assert actual['window'] == actual['worker']


@pytest.mark.asyncio
async def test_screen_dimensions(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    assert await page.evaluate("screen.width") == fp["screen"]["width"]
    assert await page.evaluate("screen.height") == fp["screen"]["height"]
    assert await page.evaluate("screen.availWidth") == fp["screen"]["avail_width"]
    assert await page.evaluate("screen.availHeight") == fp["screen"]["avail_height"]
    assert await page.evaluate("screen.colorDepth") == fp["screen"]["color_depth"]
    css = await page.evaluate(
        """screen => ({
            deviceWidth: matchMedia(
                `(device-width: ${screen.width}px)`
            ).matches,
            deviceHeight: matchMedia(
                `(device-height: ${screen.height}px)`
            ).matches,
            resolution: matchMedia(
                `(resolution: ${screen.pixel_ratio}dppx)`
            ).matches,
            color: matchMedia(
                `(color: ${Math.max(1, Math.floor(screen.color_depth / 3))})`
            ).matches,
            monochrome: matchMedia('(monochrome: 0)').matches,
        })""",
        fp["screen"],
    )
    assert css == {
        "deviceWidth": True,
        "deviceHeight": True,
        "resolution": True,
        "color": True,
        "monochrome": True,
    }


@pytest.mark.asyncio
async def test_screen_topology_uses_virtual_origin(
    browser_with_offset_window_fingerprint,
):
    page, _ = browser_with_offset_window_fingerprint
    actual = await page.evaluate("""() => ({
        availLeft: screen.availLeft,
        availTop: screen.availTop,
        screenX: window.screenX,
        screenY: window.screenY,
        screenLeft: window.screenLeft,
        screenTop: window.screenTop,
        isExtended: screen.isExtended,
    })""")
    assert actual == {
        "availLeft": 0,
        "availTop": 0,
        "screenX": 0,
        "screenY": 0,
        "screenLeft": 0,
        "screenTop": 0,
        "isExtended": False,
    }


@pytest.mark.asyncio
async def test_outer_window_size_matches_fingerprint(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    expected = _derived_window_size(fp["screen"], fp["canvas_seed"])
    actual = await page.evaluate("() => ({width: outerWidth, height: outerHeight})")
    assert actual == expected


@pytest.mark.asyncio
async def test_trusted_input_events_hide_host_window_origin(
    browser_with_offset_window_fingerprint,
):
    page, _ = browser_with_offset_window_fingerprint
    await page.evaluate("""() => {
        window.__trustedCoordinates = {};
        for (const type of ['pointermove', 'mousemove', 'wheel', 'touchstart']) {
            addEventListener(type, event => {
                const point = event.touches ? event.touches[0] : event;
                window.__trustedCoordinates[type] = {
                    screenX: point.screenX,
                    screenY: point.screenY,
                    clientX: point.clientX,
                    clientY: point.clientY,
                    trusted: event.isTrusted,
                };
            }, {once: true});
        }
    }""")
    await page.mouse.move(80, 80)
    await page.mouse.wheel(0, 1)
    cdp = await page.context.new_cdp_session(page)
    try:
        await cdp.send(
            "Input.dispatchTouchEvent",
            {
                "type": "touchStart",
                "touchPoints": [{"x": 150, "y": 160}],
            },
        )
        await cdp.send(
            "Input.dispatchTouchEvent", {"type": "touchEnd", "touchPoints": []}
        )
    finally:
        await cdp.detach()
    await page.wait_for_function(
        "Object.keys(window.__trustedCoordinates).length === 4"
    )
    actual = await page.evaluate("window.__trustedCoordinates")

    offsets = []
    for event in actual.values():
        assert event["trusted"] is True
        offsets.append(
            (
                event["screenX"] - event["clientX"],
                event["screenY"] - event["clientY"],
            )
        )
    # A virtual window starts at (0, 0). Only stable browser-chrome insets may
    # remain; the deliberately large host --window-position must not appear.
    assert all(abs(x_offset) < 64 for x_offset, _ in offsets)
    assert all(0 <= y_offset < 200 for _, y_offset in offsets)

    synthetic = await page.evaluate("""() => {
        const event = new MouseEvent('mousemove', {
            screenX: 901, screenY: 902, clientX: 11, clientY: 12,
        });
        return {screenX: event.screenX, screenY: event.screenY};
    }""")
    assert synthetic == {"screenX": 901, "screenY": 902}


@pytest.mark.asyncio
async def test_resize_observer_device_pixels_match_virtual_dpr(
    browser_with_absurd_fingerprint,
):
    page, data = browser_with_absurd_fingerprint
    dpr = data["response"]["fingerprint"]["screen"]["pixel_ratio"]
    actual = await page.evaluate("""() => new Promise(resolve => {
        const target = document.createElement('div');
        target.style.cssText = 'width:160px;height:80px;padding:0;border:0';
        document.body.append(target);
        const observer = new ResizeObserver(entries => {
            const entry = entries[0];
            const css = entry.contentBoxSize[0];
            const device = entry.devicePixelContentBoxSize[0];
            observer.disconnect();
            target.remove();
            resolve({
                cssInline: css.inlineSize,
                cssBlock: css.blockSize,
                deviceInline: device.inlineSize,
                deviceBlock: device.blockSize,
            });
        });
        observer.observe(target, {box: 'device-pixel-content-box'});
    })""")
    assert actual["deviceInline"] == round(actual["cssInline"] * dpr)
    assert actual["deviceBlock"] == round(actual["cssBlock"] * dpr)


@pytest.mark.asyncio
async def test_screen_details_exposes_one_virtual_screen(
    browser_with_screen_details_fingerprint,
):
    startup_page, data = browser_with_screen_details_fingerprint
    fp = data["response"]["fingerprint"]
    browser = startup_page.context.browser
    assert browser is not None
    browser_cdp = await browser.new_browser_cdp_session()
    browser_context_id = None

    try:
        browser_context_id = (
            await browser_cdp.send("Target.createBrowserContext")
        )["browserContextId"]
        origin = await startup_page.evaluate("location.origin")
        await browser_cdp.send(
            "Browser.grantPermissions",
            {
                "permissions": ["windowManagement"],
                "origin": origin,
                "browserContextId": browser_context_id,
            },
        )

        existing_page_ids = {
            id(candidate)
            for context in browser.contexts
            for candidate in context.pages
        }
        await browser_cdp.send(
            "Target.createTarget",
            {
                "url": startup_page.url,
                "browserContextId": browser_context_id,
            },
        )
        deadline = asyncio.get_running_loop().time() + 5
        page = None
        while asyncio.get_running_loop().time() < deadline:
            page = next(
                (
                    candidate
                    for context in browser.contexts
                    for candidate in context.pages
                    if id(candidate) not in existing_page_ids
                ),
                None,
            )
            if page is not None:
                break
            await asyncio.sleep(0.05)
        assert page is not None, "CDP-created test target was not attached"
        await page.wait_for_load_state("domcontentloaded")

        actual = await page.evaluate("""async () => {
            const permission = await navigator.permissions.query({
                name: 'window-management',
            });
            const details = await getScreenDetails();
            const current = details.currentScreen;
            return {
                permissionState: permission.state,
                screenCount: details.screens.length,
                currentIsOnlyScreen: details.screens[0] === current,
                width: current.width,
                height: current.height,
                availWidth: current.availWidth,
                availHeight: current.availHeight,
                colorDepth: current.colorDepth,
                left: current.left,
                top: current.top,
                availLeft: current.availLeft,
                availTop: current.availTop,
                devicePixelRatio: current.devicePixelRatio,
                isPrimary: current.isPrimary,
                isInternal: current.isInternal,
                label: current.label,
                hdrHeadroom: current.hdrHeadroom,
                highDynamicRangeHeadroom: current.highDynamicRangeHeadroom,
                redPrimaryX: current.redPrimaryX,
                redPrimaryY: current.redPrimaryY,
                greenPrimaryX: current.greenPrimaryX,
                greenPrimaryY: current.greenPrimaryY,
                bluePrimaryX: current.bluePrimaryX,
                bluePrimaryY: current.bluePrimaryY,
                whitePointX: current.whitePointX,
                whitePointY: current.whitePointY,
                dynamicRangeHigh: matchMedia('(dynamic-range: high)').matches,
                colorGamutP3: matchMedia('(color-gamut: p3)').matches,
                colorGamutSrgb: matchMedia('(color-gamut: srgb)').matches,
            };
        }""")
    finally:
        if browser_context_id is not None:
            await browser_cdp.send(
                "Target.disposeBrowserContext",
                {"browserContextId": browser_context_id},
            )
        await browser_cdp.detach()

    base_metrics = {
        key: actual[key]
        for key in (
            "permissionState",
            "screenCount",
            "currentIsOnlyScreen",
            "width",
            "height",
            "availWidth",
            "availHeight",
            "colorDepth",
            "left",
            "top",
            "availLeft",
            "availTop",
            "devicePixelRatio",
            "isPrimary",
            "isInternal",
            "label",
            "dynamicRangeHigh",
            "colorGamutP3",
            "colorGamutSrgb",
        )
    }
    assert base_metrics == {
        "permissionState": "granted",
        "screenCount": 1,
        "currentIsOnlyScreen": True,
        # ScreenDetailed inherits these virtualized Screen accessors.
        "width": fp["screen"]["width"],
        "height": fp["screen"]["height"],
        "availWidth": fp["screen"]["avail_width"],
        "availHeight": fp["screen"]["avail_height"],
        "colorDepth": fp["screen"]["color_depth"],
        "left": 0,
        "top": 0,
        "availLeft": 0,
        "availTop": 0,
        "devicePixelRatio": fp["screen"]["pixel_ratio"],
        "isPrimary": True,
        "isInternal": False,
        "label": "",
        "dynamicRangeHigh": False,
        "colorGamutP3": False,
        "colorGamutSrgb": True,
    }
    assert actual["hdrHeadroom"] == pytest.approx(0.0)
    assert actual["highDynamicRangeHeadroom"] == pytest.approx(1.0)
    assert actual["redPrimaryX"] == pytest.approx(0.64)
    assert actual["redPrimaryY"] == pytest.approx(0.33)
    assert actual["greenPrimaryX"] == pytest.approx(0.30)
    assert actual["greenPrimaryY"] == pytest.approx(0.60)
    assert actual["bluePrimaryX"] == pytest.approx(0.15)
    assert actual["bluePrimaryY"] == pytest.approx(0.06)
    assert actual["whitePointX"] == pytest.approx(0.3127)
    assert actual["whitePointY"] == pytest.approx(0.3290)


@pytest.mark.asyncio
async def test_screen_orientation_matches_fingerprint(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    screen = data["response"]["fingerprint"]["screen"]
    is_portrait = screen["height"] >= screen["width"]
    actual = await page.evaluate("""() => ({
        type: screen.orientation.type,
        angle: screen.orientation.angle,
        hasLegacyOrientation: 'orientation' in window,
        legacyOrientation: 'orientation' in window ? window.orientation : null,
    })""")
    assert actual == {
        "type": "portrait-primary" if is_portrait else "landscape-primary",
        "angle": 0,
        "hasLegacyOrientation": actual["hasLegacyOrientation"],
        # window.orientation is Android-only in stable Chromium.
        "legacyOrientation": 0 if actual["hasLegacyOrientation"] else None,
    }


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
async def test_webgl_uses_coherent_swiftshader_backend(
    browser_with_isolated_webgl,
):
    page, data = browser_with_isolated_webgl
    fp = data["response"]["fingerprint"]
    assert data["request"]["runtime_gpu"] == "swiftshader"
    assert fp["surface_policy"]["webgl"]["mode"] == "native"
    result = await page.evaluate("""() => {
        const canvas = document.createElement('canvas');
        const gl = canvas.getContext('webgl');
        if (!gl) return null;
        const ext = gl.getExtension('WEBGL_debug_renderer_info');
        if (!ext) return null;
        return {
            maskedVendor: gl.getParameter(gl.VENDOR),
            maskedRenderer: gl.getParameter(gl.RENDERER),
            vendor: gl.getParameter(ext.UNMASKED_VENDOR_WEBGL),
            renderer: gl.getParameter(ext.UNMASKED_RENDERER_WEBGL),
            maxTextureSize: gl.getParameter(gl.MAX_TEXTURE_SIZE),
            maxRenderbufferSize: gl.getParameter(gl.MAX_RENDERBUFFER_SIZE),
            maxCombinedTextureUnits:
                gl.getParameter(gl.MAX_COMBINED_TEXTURE_IMAGE_UNITS),
            extensions: gl.getSupportedExtensions()
        };
    }""")
    assert result is not None
    assert result["maskedVendor"] == "WebKit"
    assert result["maskedRenderer"] == "WebKit WebGL"
    assert "Google" in result["vendor"]
    assert "SwiftShader" in result["renderer"]
    assert "Apple" not in result["renderer"]
    assert "AMD" not in result["renderer"]
    assert "NVIDIA" not in result["renderer"]
    assert result["maxTextureSize"] == 8192
    assert result["maxRenderbufferSize"] == 8192
    assert result["maxCombinedTextureUnits"] == 64
    assert "WEBGL_debug_renderer_info" in result["extensions"]


@pytest.mark.asyncio
async def test_webgl_pixels_are_native_and_preserve_alpha(
    browser_with_isolated_webgl,
):
    page, data = browser_with_isolated_webgl
    fp = data["response"]["fingerprint"]
    assert fp["surface_policy"]["canvas"]["mode"] == "override"

    result = await page.evaluate("""() => {
        const read = () => {
            const canvas = document.createElement('canvas');
            canvas.width = 8;
            canvas.height = 8;
            const gl = canvas.getContext('webgl');
            gl.clearColor(0.25, 0.5, 0.75, 1.0);
            gl.clear(gl.COLOR_BUFFER_BIT);
            const pixels = new Uint8Array(8 * 8 * 4);
            gl.readPixels(0, 0, 8, 8, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
            return Array.from(pixels);
        };
        return {first: read(), second: read()};
    }""")
    assert result["first"] == result["second"]
    assert all(alpha == 255 for alpha in result["first"][3::4])


@pytest.mark.asyncio
async def test_webgpu_does_not_expose_host_adapter(browser_with_isolated_webgl):
    """Dawn must not undo WebGL isolation by selecting the host GPU."""
    page, _ = browser_with_isolated_webgl
    result = await page.evaluate("""async () => {
        if (!navigator.gpu) return {supported: false, adapter: null};
        const adapter = await navigator.gpu.requestAdapter();
        if (!adapter) return {supported: true, adapter: null};
        const info = adapter.info || {};
        return {
            supported: true,
            adapter: {
                vendor: info.vendor || '',
                architecture: info.architecture || '',
                device: info.device || '',
                description: info.description || '',
                isFallbackAdapter: adapter.isFallbackAdapter ?? null,
            },
        };
    }""")

    # Returning no adapter is a privacy-safe fallback when a platform refuses
    # software WebGPU. If an adapter is available, it must be the same bundled
    # software family used by managed WebGL, never the physical host device.
    if result["adapter"] is None:
        return
    identity = " ".join(
        str(result["adapter"][field])
        for field in ("vendor", "architecture", "device", "description")
    ).casefold()
    assert not any(
        host_vendor in identity
        for host_vendor in ("apple", "amd", "nvidia", "intel", "radeon", "geforce")
    ), f"WebGPU exposed the host adapter: {result['adapter']}"
    assert not identity.strip() or "swiftshader" in identity or "google" in identity


@pytest.mark.asyncio
async def test_canvas_noise_preserves_alpha(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    assert fp["surface_policy"]["canvas"]["mode"] == "override"

    pixel = await page.evaluate("""() => {
        const canvas = document.createElement('canvas');
        canvas.width = 1;
        canvas.height = 1;
        const ctx = canvas.getContext('2d');
        ctx.fillStyle = 'rgba(50, 100, 150, 0.5)';
        ctx.fillRect(0, 0, 1, 1);
        return Array.from(ctx.getImageData(0, 0, 1, 1).data);
    }""")
    assert pixel[3] == 128


@pytest.mark.asyncio
async def test_canvas_readback_matches_window_and_worker_offscreen(
    browser_with_fingerprint,
):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    assert fp["surface_policy"]["canvas"]["mode"] == "override"

    actual = await page.evaluate("""async () => {
        const source = [50, 100, 150, 77, 25, 75, 125, 201];
        const drawAndRead = canvas => {
            const context = canvas.getContext('2d');
            context.putImageData(new ImageData(
                new Uint8ClampedArray(source), 2, 1), 0, 0);
            return Array.from(context.getImageData(0, 0, 2, 1).data);
        };

        const htmlCanvas = document.createElement('canvas');
        htmlCanvas.width = 2;
        htmlCanvas.height = 1;
        const windowCanvas = drawAndRead(htmlCanvas);
        htmlCanvas.getContext('2d').putImageData(new ImageData(
            new Uint8ClampedArray(windowCanvas), 2, 1), 0, 0);
        const windowCanvasRoundTrip = Array.from(
            htmlCanvas.getContext('2d').getImageData(0, 0, 2, 1).data);
        const windowOffscreen = drawAndRead(new OffscreenCanvas(2, 1));

        const workerSource = `
            onmessage = () => {
                const source = [50, 100, 150, 77, 25, 75, 125, 201];
                const canvas = new OffscreenCanvas(2, 1);
                const context = canvas.getContext('2d');
                context.putImageData(new ImageData(
                    new Uint8ClampedArray(source), 2, 1), 0, 0);
                postMessage(Array.from(
                    context.getImageData(0, 0, 2, 1).data));
            };
        `;
        const workerUrl = URL.createObjectURL(new Blob(
            [workerSource], {type: 'text/javascript'}));
        const worker = new Worker(workerUrl);
        const workerOffscreen = await new Promise((resolve, reject) => {
            worker.onmessage = event => resolve(event.data);
            worker.onerror = event => reject(new Error(event.message));
            worker.postMessage(null);
        });
        worker.terminate();
        URL.revokeObjectURL(workerUrl);
        return {
            source, windowCanvas, windowCanvasRoundTrip,
            windowOffscreen, workerOffscreen,
        };
    }""")

    assert actual["windowCanvasRoundTrip"] == actual["windowCanvas"]
    assert actual["windowCanvas"] == actual["windowOffscreen"]
    assert actual["windowCanvas"] == actual["workerOffscreen"]
    assert actual["windowCanvas"][3::4] == [77, 201]
    assert actual["windowCanvas"] != actual["source"]


@pytest.mark.asyncio
async def test_canvas_overlapping_readbacks_use_source_coordinates(
    browser_with_fingerprint,
):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    assert fp["surface_policy"]["canvas"]["mode"] == "override"

    actual = await page.evaluate("""() => {
        const canvas = document.createElement('canvas');
        canvas.width = 3;
        canvas.height = 2;
        const context = canvas.getContext('2d');
        const source = new Uint8ClampedArray([
            10, 20, 30, 255, 11, 21, 31, 255, 12, 22, 32, 255,
            13, 23, 33, 255, 14, 24, 34, 255, 15, 25, 35, 255,
        ]);
        context.putImageData(new ImageData(source, 3, 2), 0, 0);
        return {
            full: Array.from(context.getImageData(0, 0, 3, 2).data),
            subrect: Array.from(context.getImageData(1, 0, 2, 1).data),
            reversed: Array.from(context.getImageData(3, 1, -2, -1).data),
        };
    }""")

    assert actual["subrect"] == actual["full"][4:12]
    assert actual["reversed"] == actual["subrect"]


@pytest.mark.asyncio
async def test_canvas_noise_covers_float_image_data_formats(
    browser_with_fingerprint,
):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    assert fp["surface_policy"]["canvas"]["mode"] == "override"

    formats = await page.evaluate("""() => {
        const source = [50, 100, 150, 255];
        const canvas = document.createElement('canvas');
        canvas.width = 1;
        canvas.height = 1;
        const context = canvas.getContext('2d');
        context.putImageData(new ImageData(
            new Uint8ClampedArray(source), 1, 1), 0, 0);
        const result = {};
        for (const pixelFormat of [
            'rgba-unorm8', 'rgba-float16', 'rgba-float32'
        ]) {
            try {
                const first = context.getImageData(0, 0, 1, 1, {
                    colorSpace: 'srgb', pixelFormat
                });
                const second = context.getImageData(0, 0, 1, 1, {
                    colorSpace: 'srgb', pixelFormat
                });
                result[pixelFormat] = {
                    values: Array.from(first.data),
                    bits: pixelFormat === 'rgba-float16'
                        ? Array.from(new Uint16Array(
                            first.data.buffer,
                            first.data.byteOffset,
                            first.data.length))
                        : pixelFormat === 'rgba-float32'
                            ? Array.from(new Uint32Array(
                                first.data.buffer,
                                first.data.byteOffset,
                                first.data.length))
                            : null,
                    baselineBits: pixelFormat === 'rgba-float16'
                        ? Array.from(new Uint16Array(
                            new Float16Array(Array.from(
                                source, value => value / 255)).buffer))
                        : pixelFormat === 'rgba-float32'
                            ? Array.from(new Uint32Array(
                                new Float32Array(Array.from(
                                    source, value => value / 255)).buffer))
                            : null,
                    deterministic:
                        Array.from(first.data).every(
                            (value, index) => value === second.data[index]),
                };
            } catch (error) {
                result[pixelFormat] = {unsupported: error.name};
            }
        }
        return result;
    }""")

    mask = (1 << 64) - 1

    def mix(value):
        value = (value + 0x9E3779B97F4A7C15) & mask
        value = ((value ^ (value >> 30)) * 0xBF58476D1CE4E5B9) & mask
        value = ((value ^ (value >> 27)) * 0x94D049BB133111EB) & mask
        return (value ^ (value >> 31)) & mask

    seed = fp["canvas_seed"] or 1
    pixel_seed = mix(seed)
    pixel_seed ^= mix(0)
    pixel_seed = mix(pixel_seed)
    pixel_seed ^= mix(0)
    pixel_seed = mix(pixel_seed) or 1
    canonical_bits = []
    state = pixel_seed
    for _ in range(3):
        state ^= (state << 13) & ((1 << 64) - 1)
        state ^= state >> 7
        state ^= (state << 17) & ((1 << 64) - 1)
        state &= (1 << 64) - 1
        canonical_bits.append(state & 1)

    unorm8 = formats["rgba-unorm8"]
    assert unorm8["deterministic"] is True
    assert unorm8["values"] == [
        (value & 0xFE) | canonical_bit
        for value, canonical_bit in zip([50, 100, 150], canonical_bits)
    ] + [255]

    float16 = formats["rgba-float16"]
    assert "unsupported" not in float16
    assert float16["deterministic"] is True
    assert float16["bits"][:3] == [
        (value & 0xFFFE) | canonical_bit
        for value, canonical_bit in zip(
            float16["baselineBits"][:3], canonical_bits
        )
    ]
    assert float16["bits"][3] == float16["baselineBits"][3]

    float32 = formats["rgba-float32"]
    assert "unsupported" not in float32
    assert float32["deterministic"] is True
    for actual_bits, baseline_bits, canonical_bit in zip(
        float32["bits"][:3], float32["baselineBits"][:3], canonical_bits
    ):
        assert actual_bits & 1 == canonical_bit
        assert abs(actual_bits - baseline_bits) <= 1
    assert float32["values"][3] == 1.0


@pytest.mark.asyncio
async def test_canvas_exports_match_readback_without_mutating_source(
    browser_with_fingerprint,
):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    assert fp["surface_policy"]["canvas"]["mode"] == "override"

    result = await page.evaluate("""async () => {
        if (typeof ImageDecoder !== 'function')
            throw new Error('ImageDecoder is required by the release gate');

        const source = new Uint8ClampedArray([
            50, 100, 150, 255, 25, 75, 125, 255
        ]);
        const decode = async encoded => {
            const bytes = encoded instanceof Blob
                ? await encoded.arrayBuffer()
                : await (await fetch(encoded)).arrayBuffer();
            const decoder = new ImageDecoder({data: bytes, type: 'image/png'});
            const frame = (await decoder.decode()).image;
            const output = new Uint8Array(
                frame.allocationSize({format: 'RGBA'}));
            await frame.copyTo(output, {format: 'RGBA'});
            frame.close();
            decoder.close();
            return Array.from(output);
        };
        const initialize = canvas => {
            const context = canvas.getContext('2d');
            context.putImageData(new ImageData(source.slice(), 2, 1), 0, 0);
            return context;
        };

        const canvas = document.createElement('canvas');
        canvas.width = 2;
        canvas.height = 1;
        const context = initialize(canvas);
        const direct = Array.from(context.getImageData(0, 0, 2, 1).data);
        const dataUrl = canvas.toDataURL('image/png');
        const dataUrlAgain = canvas.toDataURL('image/png');
        const decodedDataUrl = await decode(dataUrl);
        const htmlBlob = await new Promise((resolve, reject) => {
            canvas.toBlob(blob => blob ? resolve(blob) :
                reject(new Error('HTMLCanvasElement.toBlob returned null')),
                'image/png');
        });
        const decodedHtmlBlob = await decode(htmlBlob);
        const afterExport = Array.from(
            context.getImageData(0, 0, 2, 1).data);

        const offscreen = new OffscreenCanvas(2, 1);
        const offscreenContext = initialize(offscreen);
        const offscreenDirect = Array.from(
            offscreenContext.getImageData(0, 0, 2, 1).data);
        const decodedBlob = await decode(
            await offscreen.convertToBlob({type: 'image/png'}));

        const workerSource = `
            onmessage = async () => {
                const source = new Uint8ClampedArray([
                    50, 100, 150, 255, 25, 75, 125, 255
                ]);
                const canvas = new OffscreenCanvas(2, 1);
                const context = canvas.getContext('2d');
                context.putImageData(new ImageData(source, 2, 1), 0, 0);
                const direct = Array.from(
                    context.getImageData(0, 0, 2, 1).data);
                const blob = await canvas.convertToBlob({type: 'image/png'});
                postMessage({direct, blob});
            };
        `;
        const workerUrl = URL.createObjectURL(new Blob(
            [workerSource], {type: 'text/javascript'}));
        const worker = new Worker(workerUrl);
        const workerResult = await new Promise((resolve, reject) => {
            worker.onmessage = event => resolve(event.data);
            worker.onerror = event => reject(new Error(event.message));
            worker.postMessage(null);
        });
        worker.terminate();
        URL.revokeObjectURL(workerUrl);
        const decodedWorkerBlob = await decode(workerResult.blob);

        return {
            direct,
            decodedDataUrl,
            decodedHtmlBlob,
            afterExport,
            deterministicDataUrl: dataUrl === dataUrlAgain,
            offscreenDirect,
            decodedBlob,
            workerDirect: workerResult.direct,
            decodedWorkerBlob,
        };
    }""")

    assert result["direct"] == result["decodedDataUrl"]
    assert result["direct"] == result["decodedHtmlBlob"]
    assert result["direct"] == result["afterExport"]
    assert result["deterministicDataUrl"] is True
    assert result["offscreenDirect"] == result["decodedBlob"]
    assert result["offscreenDirect"] == result["direct"]
    assert result["workerDirect"] == result["decodedWorkerBlob"]
    assert result["workerDirect"] == result["direct"]
    assert result["direct"][3::4] == [255, 255]


@pytest.mark.asyncio
async def test_native_canvas_policy_does_not_apply_seed_noise(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    if fp["surface_policy"]["canvas"]["mode"] != "native":
        pytest.skip("fixture does not use native canvas policy")

    pixel = await page.evaluate("""() => {
        const canvas = document.createElement('canvas');
        canvas.width = 1;
        canvas.height = 1;
        const ctx = canvas.getContext('2d');
        ctx.fillStyle = 'rgb(255, 0, 0)';
        ctx.fillRect(0, 0, 1, 1);
        return Array.from(ctx.getImageData(0, 0, 1, 1).data);
    }""")
    assert pixel == [255, 0, 0, 255]


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
async def test_native_audio_policy_does_not_apply_seed_noise(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    if fp["surface_policy"]["audio"]["mode"] != "native":
        pytest.skip("fixture does not use native audio policy")

    samples = await page.evaluate("""async () => {
        const ctx = new OfflineAudioContext(1, 16, 44100);
        const rendered = await ctx.startRendering();
        return Array.from(rendered.getChannelData(0).slice(0, 8));
    }""")
    assert samples == [0, 0, 0, 0, 0, 0, 0, 0]


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
async def test_native_client_rects_policy_does_not_apply_seed_noise(
    browser_with_fingerprint,
):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    if fp["surface_policy"]["client_rects"]["mode"] != "native":
        pytest.skip("fixture does not use native client rects policy")

    rect = await page.evaluate("""() => {
        const el = document.createElement('div');
        el.style.cssText = 'position:absolute;top:10px;left:10px;width:100px;height:50px;';
        document.body.appendChild(el);
        const r = el.getBoundingClientRect();
        document.body.removeChild(el);
        return {x: r.x, y: r.y, width: r.width, height: r.height};
    }""")
    assert rect == {"x": 10, "y": 10, "width": 100, "height": 50}


@pytest.mark.asyncio
async def test_media_codecs_follow_backend_config(browser_with_fingerprint):
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]

    result = await page.evaluate("""() => {
        const audio = document.createElement('audio');
        const video = document.createElement('video');
        return {
            mp3: audio.canPlayType('audio/mpeg'),
            h264: video.canPlayType('video/mp4; codecs="avc1.42E01E"')
        };
    }""")

    assert result["mp3"] == fp["audio_codecs"]["mp3"]
    assert result["h264"] == fp["video_codecs"]["h264"]


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

    if sys.platform == 'linux':
        fonts = _expected_fixture_fonts(fp)
        await page.evaluate('''fonts => fonts.forEach((family, i) => {
            const el = document.createElement('span');
            el.id = 'catalog-font-' + i;
            el.style.font = '32px ' + JSON.stringify(family);
            const samples = {'Noto Color Emoji': '🚀', 'Noto Sans Bengali': 'ব',
                'Noto Sans Ethiopic': 'አ', 'Noto Sans Gujarati': 'ગ',
                'Noto Sans Gurmukhi': 'ਪ', 'Noto Sans Kannada': 'ಕ',
                'Noto Sans Khmer': 'ខ', 'Noto Sans Malayalam': 'മ',
                'Noto Sans Myanmar': 'မ', 'Noto Sans Sinhala': 'ස',
                'Noto Sans Tamil': 'த', 'Noto Sans Telugu': 'త'};
            el.textContent = samples[family] || (family.startsWith('Noto Sans CJK') ? '漢' :
                family === 'Lohit Devanagari' ? 'क' :
                family === 'Noto Sans Thai' ? 'ก' : 'A');
            document.body.appendChild(el);
        })''', fonts)
        cdp = await page.context.new_cdp_session(page)
        try:
            await cdp.send('DOM.enable')
            await cdp.send('CSS.enable')
            root = (await cdp.send('DOM.getDocument'))['root']['nodeId']
            for i, family in enumerate(fonts):
                node = (await cdp.send('DOM.querySelector', {
                    'nodeId': root, 'selector': f'#catalog-font-{i}',
                }))['nodeId']
                actual = (await cdp.send('CSS.getPlatformFontsForNode', {
                    'nodeId': node,
                }))['fonts']
                assert actual, family
                assert all(font['familyName'] == family and font['glyphCount'] > 0
                           for font in actual), (family, actual)
        finally:
            await cdp.detach()
        return

    detected = await page.evaluate(
        """fonts => {
        return fonts.filter(font => {
            const measureWidth = fontFamily => {
                const span = document.createElement('span');
                span.style.fontFamily = fontFamily;
                span.style.fontSize = '32px';
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
async def test_fonts_hide_non_allowlisted_system_families(browser_with_fingerprint):
    """A classic width probe must not reveal common host-only fonts."""
    page, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    candidates = [
        "Menlo",          # macOS
        "Segoe UI",       # Windows
        "DejaVu Sans",    # Linux
        "Liberation Sans",
        "Ubuntu",
    ]
    candidates = [font for font in candidates if font not in _expected_fixture_fonts(fp)]
    assert candidates, 'negative control must retain host-only font candidates'

    leaked = await page.evaluate(
        """fonts => fonts.filter(font => {
            const width = family => {
                const span = document.createElement('span');
                span.style.cssText = 'position:absolute;visibility:hidden;font-size:72px';
                span.style.fontFamily = family;
                span.textContent = 'mmmmmmmmmmlliWW@@##';
                document.body.appendChild(span);
                const value = span.getBoundingClientRect().width;
                span.remove();
                return value;
            };
            return width(`"${font}", monospace`) !== width('monospace');
        })""",
        candidates,
    )

    assert leaked == [], f"non-allowlisted host fonts leaked: {leaked}"


@pytest.mark.asyncio
async def test_fonts_block_non_allowlisted_local_sources(browser_with_fingerprint):
    """FontFace local() must not bypass the managed font allowlist."""
    page, data = browser_with_fingerprint
    allowed = {font.casefold() for font in data["response"]["fingerprint"]["fonts"]}
    candidates = [
        "serif",              # local() treats this as a literal unique name
        "Menlo-Regular",       # macOS PostScript name
        "SFMono-Regular",      # macOS PostScript name
        "SegoeUI",             # Windows PostScript name
        "Calibri",             # Windows full name
        "Consolas",            # Windows full name
        "DejaVuSans",          # Linux PostScript name
        "LiberationSans",      # Linux PostScript name
        "Ubuntu",              # Linux full name
    ]
    if sys.platform == 'linux':
        # Exact PostScript alias of the now-bundled DejaVu Sans family.
        # Its positive load is covered by the bundled-local-face regression.
        allowed.add('dejavusans')
    candidates = [font for font in candidates if font.casefold() not in allowed]

    result = await page.evaluate(
        """async names => {
            const loadLocal = async (name, index) => {
                const family = `ClawbrowserLocalProbe${index}`;
                const face = new FontFace(family, `local("${name}")`);
                try {
                    await face.load();
                    document.fonts.add(face);
                    return name;
                } catch (_) {
                    return null;
                }
            };
            const documentLoaded = (await Promise.all(
                names.map(loadLocal)
            )).filter(Boolean);

            const workerSource = `
                self.onmessage = async event => {
                    if (typeof FontFace !== 'function' || !self.fonts) {
                        self.postMessage({
                            supported: false,
                            loaded: [],
                            widthLeaked: [],
                        });
                        return;
                    }
                    const loaded = [];
                    for (let index = 0; index < event.data.length; index++) {
                        const name = event.data[index];
                        const face = new FontFace(
                            'ClawbrowserWorkerProbe' + index,
                            'local("' + name + '")'
                        );
                        try {
                            await face.load();
                            self.fonts.add(face);
                            loaded.push(name);
                        } catch (_) {}
                    }
                    const canvas = new OffscreenCanvas(512, 128);
                    const context = canvas.getContext('2d');
                    const width = family => {
                        context.font = '72px ' + family;
                        return context.measureText('mmmmmmmmmmlliWW@@##').width;
                    };
                    const fallbackWidth = width('monospace');
                    const widthLeaked = event.data.filter(name =>
                        width('"' + name + '", monospace') !== fallbackWidth
                    );
                    self.postMessage({supported: true, loaded, widthLeaked});
                };
            `;
            const workerUrl = URL.createObjectURL(
                new Blob([workerSource], {type: 'text/javascript'})
            );
            const workerResult = await new Promise((resolve, reject) => {
                const worker = new Worker(workerUrl);
                worker.onmessage = event => {
                    worker.terminate();
                    resolve(event.data);
                };
                worker.onerror = event => {
                    worker.terminate();
                    reject(new Error(event.message));
                };
                worker.postMessage(names);
            });
            URL.revokeObjectURL(workerUrl);
            return {documentLoaded, workerResult};
        }""",
        candidates,
    )

    assert result["documentLoaded"] == [], (
        f"FontFace local() exposed host fonts: {result['documentLoaded']}"
    )
    assert result["workerResult"]["supported"], (
        "dedicated-worker FontFaceSet/OffscreenCanvas surface is unavailable"
    )
    assert result["workerResult"]["loaded"] == [], (
        "worker FontFace local() exposed host fonts: "
        f"{result['workerResult']['loaded']}"
    )
    assert result["workerResult"]["widthLeaked"] == [], (
        "worker OffscreenCanvas exposed host fonts: "
        f"{result['workerResult']['widthLeaked']}"
    )


@pytest.mark.asyncio
async def test_query_local_fonts_exposes_no_host_backed_metadata(
    browser_with_fingerprint,
):
    """Protected profiles must not expose host font identities or blobs."""
    startup_page, _ = browser_with_fingerprint
    browser = startup_page.context.browser
    assert browser is not None
    browser_cdp = await browser.new_browser_cdp_session()
    browser_context_id = None

    try:
        browser_context_id = (
            await browser_cdp.send("Target.createBrowserContext")
        )["browserContextId"]
        origin = await startup_page.evaluate("location.origin")
        await browser_cdp.send(
            "Browser.grantPermissions",
            {
                "permissions": ["localFonts"],
                "origin": origin,
                "browserContextId": browser_context_id,
            },
        )
        existing_page_ids = {
            id(candidate)
            for context in browser.contexts
            for candidate in context.pages
        }
        await browser_cdp.send(
            "Target.createTarget",
            {
                "url": startup_page.url,
                "browserContextId": browser_context_id,
            },
        )
        deadline = asyncio.get_running_loop().time() + 5
        page = None
        while asyncio.get_running_loop().time() < deadline:
            page = next(
                (
                    candidate
                    for context in browser.contexts
                    for candidate in context.pages
                    if id(candidate) not in existing_page_ids
                ),
                None,
            )
            if page is not None:
                break
            await asyncio.sleep(0.05)
        assert page is not None, "CDP-created local-font test target was not attached"
        await page.wait_for_load_state("domcontentloaded")
        result = await page.evaluate("""async () => {
            if (typeof queryLocalFonts !== 'function') {
                return {supported: false, rows: []};
            }
            const rows = await queryLocalFonts();
            return {
                supported: true,
                rows: rows.map(row => ({
                    postscriptName: row.postscriptName,
                    fullName: row.fullName,
                    family: row.family,
                    style: row.style,
                })),
            };
        }""")
    finally:
        if browser_context_id is not None:
            await browser_cdp.send(
                "Target.disposeBrowserContext",
                {"browserContextId": browser_context_id},
            )
        await browser_cdp.detach()

    assert result["supported"] is True, (
        "queryLocalFonts must be exercised by the desktop release gate"
    )
    assert result["rows"] == [], (
        f"queryLocalFonts exposed host-backed font metadata: {result['rows']}"
    )


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
    assert json.loads(font_check["expected"]) == _expected_fixture_fonts(fp)
    assert json.loads(font_check["actual"]) == _expected_fixture_fonts(fp)
