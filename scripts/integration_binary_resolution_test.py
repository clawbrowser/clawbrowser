#!/usr/bin/env python3
import importlib.util
import os
import sys
import tempfile
import types
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
RUNNER_PATH = REPO_ROOT / "clawbrowser/test/integration/run_integration_tests.py"
CONFTEST_PATH = REPO_ROOT / "clawbrowser/test/integration/conftest.py"


def _load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def _load_conftest_module():
    pytest_asyncio_stub = types.ModuleType("pytest_asyncio")
    pytest_asyncio_stub.fixture = lambda func=None, *args, **kwargs: func
    playwright_stub = types.ModuleType("playwright")
    playwright_async_api_stub = types.ModuleType("playwright.async_api")
    playwright_async_api_stub.async_playwright = object()
    playwright_stub.async_api = playwright_async_api_stub

    original = {
        "pytest_asyncio": sys.modules.get("pytest_asyncio"),
        "playwright": sys.modules.get("playwright"),
        "playwright.async_api": sys.modules.get("playwright.async_api"),
    }
    sys.modules["pytest_asyncio"] = pytest_asyncio_stub
    sys.modules["playwright"] = playwright_stub
    sys.modules["playwright.async_api"] = playwright_async_api_stub
    try:
        return _load_module("integration_conftest_test", CONFTEST_PATH)
    finally:
        for key, value in original.items():
            if value is None:
                sys.modules.pop(key, None)
            else:
                sys.modules[key] = value


class IntegrationBinaryResolutionTest(unittest.TestCase):
    def test_runner_rejects_chromium_bundle(self):
        module = _load_module("integration_runner_test", RUNNER_PATH)
        with tempfile.TemporaryDirectory() as tmp_dir:
            cwd = Path(tmp_dir)
            chromium_binary = cwd / "out/CBFast/Chromium.app/Contents/MacOS/Chromium"
            chromium_binary.parent.mkdir(parents=True)
            chromium_binary.write_text("")
            chromium_binary.chmod(0o755)

            previous = Path.cwd()
            os.chdir(cwd)
            try:
                self.assertIsNone(module._default_browser_binary())
            finally:
                os.chdir(previous)

    def test_runner_keeps_clawbrowser_bundle_support(self):
        module = _load_module("integration_runner_test_claw", RUNNER_PATH)
        with tempfile.TemporaryDirectory() as tmp_dir:
            cwd = Path(tmp_dir)
            claw_binary = cwd / "out/CBFast/Clawbrowser.app/Contents/MacOS/Clawbrowser"
            claw_binary.parent.mkdir(parents=True)
            claw_binary.write_text("")
            claw_binary.chmod(0o755)

            previous = Path.cwd()
            os.chdir(cwd)
            try:
                self.assertEqual(
                    module._default_browser_binary(),
                    str(claw_binary.resolve()),
                )
            finally:
                os.chdir(previous)

    def test_conftest_rejects_linux_chrome_binary(self):
        module = _load_conftest_module()
        with tempfile.TemporaryDirectory() as tmp_dir:
            workspace_root = Path(tmp_dir)
            chrome_binary = workspace_root / "out/Default/chrome"
            chrome_binary.parent.mkdir(parents=True)
            chrome_binary.write_text("")
            chrome_binary.chmod(0o755)

            original_workspace_root = module.WORKSPACE_ROOT
            module.WORKSPACE_ROOT = workspace_root
            try:
                with self.assertRaises(FileNotFoundError):
                    module._resolve_browser_binary()
            finally:
                module.WORKSPACE_ROOT = original_workspace_root


if __name__ == "__main__":
    unittest.main()
