"""CLI smoke tests for clawbrowser."""

import json
import os
import subprocess
import tempfile
from pathlib import Path

import pytest
from conftest import (
    HOST_PROFILE_PLATFORM,
    TEST_API_KEY,
    _config_dir,
    _launch_browser_with_details,
    _resolve_browser_binary,
    _seed_config,
    _seed_profile,
)


def run_clawbrowser(args, env):
    """Run clawbrowser and return (stdout, stderr, returncode)."""
    process = subprocess.Popen(
        args,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    stdout, stderr = process.communicate()
    return stdout, stderr, process.returncode


def test_cli_list_empty():
    """Test --list when no profiles are cached."""
    binary = _resolve_browser_binary()
    with tempfile.TemporaryDirectory(prefix="clawbrowser-cli-", ignore_cleanup_errors=True) as temp_home:
        home_dir = Path(temp_home)
        config_dir = _config_dir(home_dir)
        _seed_config(config_dir, "http://127.0.0.1:0", TEST_API_KEY)

        env = {**os.environ, "HOME": str(home_dir), "CLAWBROWSER_CONFIG_DIR": str(config_dir)}
        stdout, stderr, code = run_clawbrowser([binary, "--list"], env)

        assert code == 0
        assert "No cached profiles found" in stdout or stdout.strip() == ""


def test_cli_list_with_profiles():
    """Test --list when profiles are cached."""
    binary = _resolve_browser_binary()
    with tempfile.TemporaryDirectory(prefix="clawbrowser-cli-", ignore_cleanup_errors=True) as temp_home:
        home_dir = Path(temp_home)
        config_dir = _config_dir(home_dir)
        _seed_config(config_dir, "http://127.0.0.1:0", TEST_API_KEY)
        _seed_profile(config_dir, "valid_fingerprint.json")

        env = {**os.environ, "HOME": str(home_dir), "CLAWBROWSER_CONFIG_DIR": str(config_dir)}
        stdout, stderr, code = run_clawbrowser([binary, "--list"], env)

        assert code == 0
        assert "test_profile" in stdout


def test_cli_list_json():
    """Test --list --output=json."""
    binary = _resolve_browser_binary()
    with tempfile.TemporaryDirectory(prefix="clawbrowser-cli-", ignore_cleanup_errors=True) as temp_home:
        home_dir = Path(temp_home)
        config_dir = _config_dir(home_dir)
        _seed_config(config_dir, "http://127.0.0.1:0", TEST_API_KEY)
        _seed_profile(config_dir, "valid_fingerprint.json")

        env = {**os.environ, "HOME": str(home_dir), "CLAWBROWSER_CONFIG_DIR": str(config_dir)}
        stdout, stderr, code = run_clawbrowser([binary, "--list", "--output=json"], env)

        assert code == 0
        data = json.loads(stdout)
        assert isinstance(data, list)
        assert any(p["id"] == "test_profile" for p in data)


@pytest.mark.asyncio
async def test_cli_verbose_smoke():
    """Test that --verbose enables startup logging during a normal launch."""
    async with _launch_browser_with_details(
        fixture_name=None,
        fingerprint_id="verbose_profile",
        backend_mode="mock",
        skip_verify=True,
        extra_browser_args=[
            "--regenerate",
            "--verbose",
            "--enable-logging=stderr",
        ],
    ) as launch:
        assert launch["page"].url.startswith("http://127.0.0.1:")
        log_contents = launch["browser_log_path"].read_text(
            encoding="utf-8", errors="replace"
        )
        assert "[clawbrowser] starting with args:" in log_contents


@pytest.mark.asyncio
async def test_cli_targeting_flags_smoke():
    """Test that targeting flags propagate into the generated profile request."""
    source_fixture = Path(__file__).resolve().parents[1] / "fixtures" / "valid_fingerprint.json"
    source_envelope = json.loads(source_fixture.read_text(encoding="utf-8"))
    fixture_data = {
        "request": {
            "platform": HOST_PROFILE_PLATFORM,
            "browser": "chrome",
            "country": "DE",
            "city": "Berlin",
            "connection_type": "mobile",
        },
        "response": {
            "fingerprint": source_envelope["response"]["fingerprint"],
        },
    }

    with tempfile.TemporaryDirectory(prefix="clawbrowser-targeting-", ignore_cleanup_errors=True) as temp_dir:
        fixture_path = Path(temp_dir) / "fingerprints.json"
        fixture_path.write_text(json.dumps(fixture_data) + "\n", encoding="utf-8")
        async with _launch_browser_with_details(
            fixture_name=None,
            fingerprint_id="targeting_profile",
            backend_mode="mock",
            skip_verify=True,
            fingerprints_fixture_path=fixture_path,
            extra_browser_args=[
                "--regenerate",
                "--country=DE",
                "--city=Berlin",
                "--connection-type=mobile",
            ],
        ) as launch:
            saved_profile = launch["fingerprint_data"]
            assert saved_profile is not None
            request = saved_profile["request"]
            assert request["browser"] == "chrome"
            assert request["country"] == "DE"
            assert request["city"] == "Berlin"
            assert request["connection_type"] == "mobile"
