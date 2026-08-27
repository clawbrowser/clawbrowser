#!/usr/bin/env python3
"""Extract the canonical browser schema artifact from the OpenAPI spec.

Usage:
    python3 extract_schemas.py [--openapi PATH] [--output FILE]

The output is a single deterministic JSON artifact that contains the browser
component's relevant schema subset. It is the canonical generator input for
`clawbrowser/generated/fingerprint_types.{h,cc}`.
"""

import argparse
import json
import pathlib
import sys

try:
    import yaml
except ImportError:
    print("PyYAML is required: pip install pyyaml", file=sys.stderr)
    sys.exit(1)

SCHEMA_ORDER = [
    "GenerateRequest",
    "GenerateResponse",
    "Fingerprint",
    "UserAgentData",
    "ClientHintBrand",
    "SurfacePolicy",
    "SurfacePolicyRule",
    "Screen",
    "Hardware",
    "WebGL",
    "MediaDevice",
    "Plugin",
    "Battery",
    "ProxyConfig",
    "VerifyProxyRequest",
    "VerifyProxyResponse",
]

ENTRYPOINTS = [
    "GenerateRequest",
    "GenerateResponse",
    "Fingerprint",
    "ProxyConfig",
    "VerifyProxyRequest",
    "VerifyProxyResponse",
]


def extract(openapi_path: pathlib.Path, output_path: pathlib.Path) -> None:
    with openapi_path.open() as f:
        spec = yaml.safe_load(f)

    schemas = spec.get("components", {}).get("schemas", {})
    missing = [name for name in SCHEMA_ORDER if name not in schemas]
    if missing:
        print(
            "ERROR: OpenAPI spec is missing required browser schemas: "
            + ", ".join(missing),
            file=sys.stderr,
        )
        sys.exit(1)

    artifact = {
        "version": 1,
        "generated_from": "api/openapi.yaml",
        "entrypoints": ENTRYPOINTS,
        "schemas": {name: schemas[name] for name in SCHEMA_ORDER},
    }

    output_path.parent.mkdir(parents=True, exist_ok=True)
    # Match generate_browser_types.py: pin the encoding and force LF so the
    # artifact is byte-identical across platforms. A bare open() writes CRLF on
    # Windows, which makes check_generated_fresh.sh's cmp report a false STALE.
    with output_path.open("w", encoding="utf-8", newline="\n") as f:
        json.dump(artifact, f, indent=2)
        f.write("\n")

    print(f"Wrote canonical browser schema artifact to {output_path}")


def main() -> None:
    repo_root = pathlib.Path(__file__).resolve().parents[2]
    default_openapi = repo_root / "api" / "openapi.yaml"
    default_output = pathlib.Path(__file__).resolve().parent / "browser_schema.json"

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--openapi",
        type=pathlib.Path,
        default=default_openapi,
        help=f"Path to openapi.yaml (default: {default_openapi})",
    )
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=default_output,
        help=f"Output path for the canonical browser schema artifact (default: {default_output})",
    )
    args = parser.parse_args()

    if not args.openapi.exists():
        print(f"ERROR: OpenAPI spec not found: {args.openapi}", file=sys.stderr)
        sys.exit(1)

    extract(args.openapi, args.output)


if __name__ == "__main__":
    main()
