#!/usr/bin/env python3
"""Run clawbrowser integration tests."""

import os
import subprocess
import sys
from pathlib import Path


def _default_browser_binary():
    candidate = Path("out/CBFast/Chromium.app/Contents/MacOS/Chromium")
    if candidate.exists():
        return str(candidate.resolve())
    return None


def main():
    env = os.environ.copy()
    if "CLAWBROWSER_BINARY" not in env:
        default_binary = _default_browser_binary()
        if default_binary is not None:
            env["CLAWBROWSER_BINARY"] = default_binary

    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "pytest",
            "clawbrowser/test/integration/",
            "-v",
            "--tb=short",
            *sys.argv[1:],
        ],
        cwd=".",  # Run from chromium/src root
        env=env,
    )
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
