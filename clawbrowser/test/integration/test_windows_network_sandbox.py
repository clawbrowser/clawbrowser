"""Windows: a fresh managed profile starts with a sandboxed network service.

Regression for patch 064. The LPAC network service ran the startup cursor
hack, USER32 failed to initialize (1114) and the service crashed before a
fresh profile's first navigation committed. Chromium then records
net.network_service_failed_launch_major_version in Local State and later
launches fall back to an unsandboxed network service, so a pass must also
prove that no fallback happened and the service really is an AppContainer.

Run against an INSTALLED artifact: the installer grants the
lpacChromeInstallFiles capability on the install directory; an unpacked
archive fails the AppContainer access check (SBOX_ERROR 45) by design.
The AppContainer assertion holds while the build enables NetworkServiceSandbox
(currently via the field trial testing config); revisit it if that changes.
"""

import ctypes
import json
import os
import subprocess
import sys
from ctypes import wintypes
from pathlib import Path

import pytest

from conftest import _launch_browser_with_details

pytestmark = pytest.mark.skipif(sys.platform != "win32",
                                reason="Windows network service sandbox")

# variations::HashName("network.mojom.NetworkService") as reported by the
# sparse ChildProcess.*.UtilityProcessHash histograms (int32).
NETWORK_SERVICE_HASH = -2043041185
FAILED_LAUNCH_PREF = "network_service_failed_launch_major_version"


def _network_service_pids(browser_pid):
    script = (
        "Get-CimInstance Win32_Process -Filter \"ParentProcessId=%d\" | "
        "Where-Object { $_.CommandLine -match "
        "'--utility-sub-type=network.mojom.NetworkService' } | "
        "Select-Object -ExpandProperty ProcessId | ConvertTo-Json" % browser_pid)
    out = subprocess.run(["powershell", "-NoProfile", "-Command", script],
                         capture_output=True, text=True, check=True).stdout
    value = json.loads(out) if out.strip() else []
    return value if isinstance(value, list) else [value]


def _is_app_container(pid):
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    advapi32 = ctypes.WinDLL("advapi32", use_last_error=True)
    kernel32.OpenProcess.restype = wintypes.HANDLE
    process = kernel32.OpenProcess(0x1000, False, pid)  # QUERY_LIMITED_INFORMATION
    assert process, f"OpenProcess({pid}) failed: {ctypes.get_last_error()}"
    token = wintypes.HANDLE()
    try:
        assert advapi32.OpenProcessToken(process, 0x0008, ctypes.byref(token)), \
            f"OpenProcessToken({pid}) failed: {ctypes.get_last_error()}"
        value = wintypes.DWORD()
        returned = wintypes.DWORD()
        assert advapi32.GetTokenInformation(
            token, 29, ctypes.byref(value), ctypes.sizeof(value),  # TokenIsAppContainer
            ctypes.byref(returned)), ctypes.get_last_error()
        return bool(value.value)
    finally:
        if token:
            kernel32.CloseHandle(token)
        kernel32.CloseHandle(process)


def _sparse_samples(histograms, name):
    for histogram in histograms:
        if histogram["name"] == name:
            return {bucket["low"] for bucket in histogram["buckets"]}
    return set()


@pytest.mark.asyncio
async def test_fresh_profile_network_service_is_sandboxed_and_alive():
    # The startup URL is the mock origin: the first navigation needs the
    # network service, which is exactly what crashed on a fresh profile.
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json", skip_verify=True,
        headless=False,
    ) as launch:
        page = launch["page"]
        assert page.url.endswith("/__blank"), page.url

        browser = page.context.browser
        session = await browser.new_browser_cdp_session()
        processes = (await session.send("SystemInfo.getProcessInfo"))["processInfo"]
        browser_pid = next(p["id"] for p in processes if p["type"] == "browser")
        histograms = (await session.send(
            "Browser.getHistograms", {"query": "ChildProcess"}))["histograms"]

        for name in ("ChildProcess.Crashed.UtilityProcessHash",
                     "ChildProcess.LaunchFailed.UtilityProcessHash"):
            assert NETWORK_SERVICE_HASH not in _sparse_samples(histograms, name), name

        pids = _network_service_pids(browser_pid)
        assert pids, "no network service process found"
        assert all(_is_app_container(pid) for pid in pids), \
            "network service is not running in an AppContainer"

        await page.goto("chrome://version")
        profile_path = Path(await page.locator("#profile_path").inner_text())
        local_state = json.loads(
            (profile_path.parent / "Local State").read_text(encoding="utf-8"))
        assert FAILED_LAUNCH_PREF not in local_state.get("net", {}), \
            "network service failed to launch sandboxed and fell back"
