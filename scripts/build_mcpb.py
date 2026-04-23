#!/usr/bin/env python3
"""Build the Claude Desktop MCP bundle for Clawbrowser."""

from __future__ import annotations

import argparse
import json
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXTENSION_ROOT = ROOT / "claude-desktop-extension"
DEFAULT_OUTPUT = ROOT / "clawbrowser-desktop-extension.mcpb"


def die(message: str) -> None:
    print(f"[build-mcpb] ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def load_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        die(f"invalid JSON in {path}: {exc}")


def validate_inputs() -> None:
    required = [
        EXTENSION_ROOT / "manifest.json",
        EXTENSION_ROOT / "icon.png",
        EXTENSION_ROOT / "server/index.js",
        ROOT / "bin/clawbrowser",
        ROOT / "bin/clawbrowser-mcp",
        ROOT / "package.json",
    ]

    for path in required:
        if not path.exists():
            die(f"required source file missing: {path}")

    package_version = load_json(ROOT / "package.json").get("version")
    manifest_version = load_json(EXTENSION_ROOT / "manifest.json").get("version")
    if package_version != manifest_version:
        die(
            "version mismatch between package.json and "
            f"{EXTENSION_ROOT / 'manifest.json'}"
        )


def stage_bundle(stage_root: Path) -> None:
    mapping = [
        (EXTENSION_ROOT / "manifest.json", stage_root / "manifest.json"),
        (EXTENSION_ROOT / "icon.png", stage_root / "icon.png"),
        (EXTENSION_ROOT / "server/index.js", stage_root / "server/index.js"),
        (ROOT / "bin/clawbrowser", stage_root / "bin/clawbrowser"),
        (ROOT / "bin/clawbrowser-mcp", stage_root / "bin/clawbrowser-mcp"),
    ]

    for source, destination in mapping:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


def add_tree_to_zip(zip_file: zipfile.ZipFile, root: Path) -> None:
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        zip_file.write(path, path.relative_to(root).as_posix())


def build_bundle(output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="clawbrowser-mcpb-") as tmp:
        stage_root = Path(tmp) / "bundle"
        stage_root.mkdir(parents=True, exist_ok=True)
        stage_bundle(stage_root)

        with zipfile.ZipFile(
            output,
            mode="w",
            compression=zipfile.ZIP_DEFLATED,
            compresslevel=9,
        ) as bundle:
            add_tree_to_zip(bundle, stage_root)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build the Clawbrowser Claude Desktop extension bundle.",
    )
    parser.add_argument(
        "--output",
        default=str(DEFAULT_OUTPUT),
        help=f"Output .mcpb file (default: {DEFAULT_OUTPUT})",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    validate_inputs()
    output = Path(args.output).expanduser().resolve()
    build_bundle(output)
    print(output)


if __name__ == "__main__":
    main()
