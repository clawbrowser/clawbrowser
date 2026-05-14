#!/usr/bin/env python3
"""Generate Clawbrowser-friendly browser types from browser_schema.json."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from dataclasses import dataclass
from typing import Any


SCHEMA_ORDER = [
    "GenerateRequest",
    "Screen",
    "Hardware",
    "WebGL",
    "MediaDevice",
    "Plugin",
    "Battery",
    "ProxyConfig",
    "Fingerprint",
    "GenerateResponse",
    "ProxyCredentials",
    "VerifyProxyRequest",
    "VerifyProxyResponse",
]


@dataclass
class FieldSpec:
    name: str
    kind: str
    required: bool
    type_name: str | None = None

    @property
    def cpp_type(self) -> str:
        if self.kind == "string":
            return "std::string" if self.required else "std::optional<std::string>"
        if self.kind == "int":
            return "int" if self.required else "std::optional<int>"
        if self.kind == "double":
            return "double" if self.required else "std::optional<double>"
        if self.kind == "bool":
            return "bool" if self.required else "std::optional<bool>"
        if self.kind == "ref":
            return self.type_name if self.required else f"std::optional<{self.type_name}>"
        if self.kind == "vector_string":
            return "std::vector<std::string>"
        if self.kind == "vector_ref":
            return f"std::vector<{self.type_name}>"
        raise ValueError(f"unsupported field kind: {self.kind}")

    @property
    def default_initializer(self) -> str:
        if self.kind == "int" and self.required:
            return " = 0"
        if self.kind == "double" and self.required:
            return " = 0.0"
        if self.kind == "bool" and self.required:
            return " = false"
        return ""


@dataclass
class TypeSpec:
    name: str
    fields: list[FieldSpec]
    required_names: list[str]
    is_complex: bool = False


def load_artifact(path: pathlib.Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def ref_name(ref: str) -> str:
    return ref.rsplit("/", 1)[-1]


def parse_field(type_name: str, field_name: str, field_schema: dict[str, Any], required: bool) -> FieldSpec:
    if type_name == "VerifyProxyRequest" and field_name == "proxy":
        return FieldSpec(field_name, "ref", required, "ProxyCredentials")

    if "$ref" in field_schema:
        return FieldSpec(field_name, "ref", required, ref_name(field_schema["$ref"]))

    schema_type = field_schema.get("type")
    if schema_type == "string":
        return FieldSpec(field_name, "string", required)
    if schema_type == "integer":
        return FieldSpec(field_name, "int", required)
    if schema_type == "number":
        return FieldSpec(field_name, "double", required)
    if schema_type == "boolean":
        return FieldSpec(field_name, "bool", required)
    if schema_type == "array":
        items = field_schema["items"]
        if "$ref" in items:
            return FieldSpec(field_name, "vector_ref", required, ref_name(items["$ref"]))
        if items.get("type") == "string":
            return FieldSpec(field_name, "vector_string", required)
    raise ValueError(f"unsupported field schema for {type_name}.{field_name}: {field_schema}")


def build_specs(artifact: dict[str, Any]) -> list[TypeSpec]:
    schemas = artifact["schemas"]
    specs: list[TypeSpec] = []

    for name in SCHEMA_ORDER:
        if name == "ProxyCredentials":
            schema = schemas["VerifyProxyRequest"]["properties"]["proxy"]
        else:
            schema = schemas[name]

        properties = schema.get("properties", {})
        required_names = schema.get("required", [])
        fields = [
            parse_field(name, field_name, field_schema, field_name in required_names)
            for field_name, field_schema in properties.items()
        ]
        specs.append(TypeSpec(name=name, fields=fields, required_names=required_names))

    specs_by_name = {spec.name: spec for spec in specs}
    complexity_cache: dict[str, bool] = {}

    def is_field_complex(field: FieldSpec) -> bool:
        if not field.required:
            return True
        if field.kind in {"string", "vector_string", "vector_ref"}:
            return True
        if field.kind == "ref":
            return is_type_complex(specs_by_name[field.type_name])
        return False

    def is_type_complex(spec: TypeSpec) -> bool:
        if spec.name in complexity_cache:
            return complexity_cache[spec.name]

        complexity_cache[spec.name] = any(
            is_field_complex(field) for field in spec.fields
        )
        return complexity_cache[spec.name]

    for spec in specs:
        spec.is_complex = is_type_complex(spec)

    return specs


def emit_special_member_declarations(spec: TypeSpec) -> list[str]:
    if not spec.is_complex:
        return []

    return [
        f"  {spec.name}();",
        f"  {spec.name}(const {spec.name}&);",
        f"  {spec.name}& operator=(const {spec.name}&);",
        f"  {spec.name}({spec.name}&&);",
        f"  {spec.name}& operator=({spec.name}&&);",
        f"  ~{spec.name}();",
        "",
    ]


def emit_special_member_definitions(spec: TypeSpec) -> list[str]:
    if not spec.is_complex:
        return []

    return [
        f"{spec.name}::{spec.name}() = default;",
        f"{spec.name}::{spec.name}(const {spec.name}&) = default;",
        f"{spec.name}& {spec.name}::operator=(const {spec.name}&) = default;",
        f"{spec.name}::{spec.name}({spec.name}&&) = default;",
        f"{spec.name}& {spec.name}::operator=({spec.name}&&) = default;",
        f"{spec.name}::~{spec.name}() = default;",
        "",
    ]


def emit_header(specs: list[TypeSpec]) -> str:
    lines = [
        "// Copyright 2026 The Clawbrowser Authors. All rights reserved.",
        "// Use of this source code is governed by a BSD-style license that can be",
        "// found in the LICENSE file.",
        "//",
        "// AUTO-GENERATED FILE — DO NOT EDIT MANUALLY.",
        "// Source: clawbrowser/schemas/browser_schema.json",
        "// See clawbrowser/generated/README.md for re-generation instructions.",
        "",
        "#ifndef CLAWBROWSER_GENERATED_FINGERPRINT_TYPES_H_",
        "#define CLAWBROWSER_GENERATED_FINGERPRINT_TYPES_H_",
        "",
        "#include <optional>",
        "#include <string>",
        "#include <vector>",
        "",
        '#include "base/types/expected.h"',
        '#include "base/values.h"',
        "",
        "namespace clawbrowser {",
        "",
    ]

    for spec in specs:
        lines.extend(
            [
                f"struct {spec.name} {{",
            ]
        )
        lines.extend(emit_special_member_declarations(spec))
        for field in spec.fields:
            lines.append(f"  {field.cpp_type} {field.name}{field.default_initializer};")
        lines.extend(
            [
                "",
                f"  static base::expected<{spec.name}, std::string> FromDict(",
                "      const base::DictValue& dict);",
                f"  static base::expected<{spec.name}, std::string> FromJson(",
                "      const std::string& json);",
                "  base::DictValue ToDict() const;",
                "  std::string ToJson() const;",
                "};",
                "",
            ]
        )

    lines.extend(
        [
            "}  // namespace clawbrowser",
            "",
            "#endif  // CLAWBROWSER_GENERATED_FINGERPRINT_TYPES_H_",
            "",
        ]
    )
    return "\n".join(lines)


def emit_required_parse(field: FieldSpec) -> list[str]:
    lines: list[str] = []
    if field.kind == "string":
        lines.extend(
            [
                f'  auto {field.name} = RequireString(dict, "{field.name}");',
                f"  if (!{field.name}.has_value()) return base::unexpected({field.name}.error());",
                f"  value.{field.name} = *{field.name};",
            ]
        )
    elif field.kind == "int":
        lines.extend(
            [
                f'  auto {field.name} = RequireInt(dict, "{field.name}");',
                f"  if (!{field.name}.has_value()) return base::unexpected({field.name}.error());",
                f"  value.{field.name} = *{field.name};",
            ]
        )
    elif field.kind == "double":
        lines.extend(
            [
                f'  auto {field.name} = RequireDouble(dict, "{field.name}");',
                f"  if (!{field.name}.has_value()) return base::unexpected({field.name}.error());",
                f"  value.{field.name} = *{field.name};",
            ]
        )
    elif field.kind == "bool":
        lines.extend(
            [
                f'  auto {field.name} = RequireBool(dict, "{field.name}");',
                f"  if (!{field.name}.has_value()) return base::unexpected({field.name}.error());",
                f"  value.{field.name} = *{field.name};",
            ]
        )
    elif field.kind == "ref":
        lines.extend(
            [
                f'  auto {field.name}_dict = RequireDict(dict, "{field.name}");',
                f"  if (!{field.name}_dict.has_value()) return base::unexpected({field.name}_dict.error());",
                f"  auto {field.name} = {field.type_name}::FromDict(**{field.name}_dict);",
                f"  if (!{field.name}.has_value()) return base::unexpected({field.name}.error());",
                f"  value.{field.name} = std::move(*{field.name});",
            ]
        )
    elif field.kind == "vector_string":
        lines.extend(
            [
                f'  const base::ListValue* {field.name}_list = dict.FindList("{field.name}");',
                f'  if (!{field.name}_list) return base::unexpected("missing required array field: {field.name}");',
                f"  for (const base::Value& item : *{field.name}_list) {{",
                "    if (!item.is_string()) {",
                f'      return base::unexpected("{field.name} array contains non-string element");',
                "    }",
                f"    value.{field.name}.push_back(item.GetString());",
                "  }",
            ]
        )
    elif field.kind == "vector_ref":
        lines.extend(
            [
                f'  const base::ListValue* {field.name}_list = dict.FindList("{field.name}");',
                f'  if (!{field.name}_list) return base::unexpected("missing required array field: {field.name}");',
                f"  for (const base::Value& item : *{field.name}_list) {{",
                "    if (!item.is_dict()) {",
                f'      return base::unexpected("{field.name} array contains non-object element");',
                "    }",
                f"    auto parsed_item = {field.type_name}::FromDict(item.GetDict());",
                "    if (!parsed_item.has_value()) return base::unexpected(parsed_item.error());",
                f"    value.{field.name}.push_back(std::move(*parsed_item));",
                "  }",
            ]
        )
    else:
        raise ValueError(field.kind)
    return lines


def emit_optional_parse(field: FieldSpec) -> list[str]:
    lines: list[str] = []
    if field.kind == "string":
        lines.append(f'  if (const std::string* parsed = dict.FindString("{field.name}")) value.{field.name} = *parsed;')
    elif field.kind == "int":
        lines.append(f'  if (std::optional<int> parsed = dict.FindInt("{field.name}")) value.{field.name} = *parsed;')
    elif field.kind == "double":
        lines.append(f'  if (std::optional<double> parsed = FindOptionalDouble(dict, "{field.name}")) value.{field.name} = *parsed;')
    elif field.kind == "bool":
        lines.append(f'  if (std::optional<bool> parsed = dict.FindBool("{field.name}")) value.{field.name} = *parsed;')
    elif field.kind == "ref":
        lines.extend(
            [
                f'  if (const base::DictValue* parsed_dict = dict.FindDict("{field.name}")) {{',
                f"    auto parsed = {field.type_name}::FromDict(*parsed_dict);",
                "    if (!parsed.has_value()) return base::unexpected(parsed.error());",
                f"    value.{field.name} = std::move(*parsed);",
                "  }",
            ]
        )
    elif field.kind == "vector_string":
        lines.extend(
            [
                f'  if (const base::ListValue* parsed_list = dict.FindList("{field.name}")) {{',
                "    for (const base::Value& item : *parsed_list) {",
                "      if (!item.is_string()) {",
                f'        return base::unexpected("{field.name} array contains non-string element");',
                "      }",
                f"      value.{field.name}.push_back(item.GetString());",
                "    }",
                "  }",
            ]
        )
    elif field.kind == "vector_ref":
        lines.extend(
            [
                f'  if (const base::ListValue* parsed_list = dict.FindList("{field.name}")) {{',
                "    for (const base::Value& item : *parsed_list) {",
                "      if (!item.is_dict()) {",
                f'        return base::unexpected("{field.name} array contains non-object element");',
                "      }",
                f"      auto parsed = {field.type_name}::FromDict(item.GetDict());",
                "      if (!parsed.has_value()) return base::unexpected(parsed.error());",
                f"      value.{field.name}.push_back(std::move(*parsed));",
                "    }",
                "  }",
            ]
        )
    else:
        raise ValueError(field.kind)
    return lines


def emit_to_dict(field: FieldSpec) -> list[str]:
    if field.kind in {"string", "int", "double", "bool"}:
        if field.required:
            return [f'  dict.Set("{field.name}", {field.name});']
        return [f'  if ({field.name}.has_value()) dict.Set("{field.name}", *{field.name});']
    if field.kind == "ref":
        if field.required:
            return [f'  dict.Set("{field.name}", {field.name}.ToDict());']
        return [f'  if ({field.name}.has_value()) dict.Set("{field.name}", {field.name}->ToDict());']
    if field.kind == "vector_string":
        lines = [
            "  {",
            "    base::ListValue list;",
            f"    for (const auto& item : {field.name}) list.Append(item);",
        ]
        if field.required:
            lines.append(f'    dict.Set("{field.name}", std::move(list));')
        else:
            lines.extend(
                [
                    f"    if (!{field.name}.empty()) dict.Set(\"{field.name}\", std::move(list));",
                ]
            )
        lines.append("  }")
        return lines
    if field.kind == "vector_ref":
        lines = [
            "  {",
            "    base::ListValue list;",
            f"    for (const auto& item : {field.name}) list.Append(item.ToDict());",
        ]
        if field.required:
            lines.append(f'    dict.Set("{field.name}", std::move(list));')
        else:
            lines.extend(
                [
                    f"    if (!{field.name}.empty()) dict.Set(\"{field.name}\", std::move(list));",
                ]
            )
        lines.append("  }")
        return lines
    raise ValueError(field.kind)


def emit_source(specs: list[TypeSpec]) -> str:
    lines = [
        "// Copyright 2026 The Clawbrowser Authors. All rights reserved.",
        "// Use of this source code is governed by a BSD-style license that can be",
        "// found in the LICENSE file.",
        "//",
        "// AUTO-GENERATED FILE — DO NOT EDIT MANUALLY.",
        "// Source: clawbrowser/schemas/browser_schema.json",
        "// See clawbrowser/generated/README.md for re-generation instructions.",
        "",
        '#include "clawbrowser/generated/fingerprint_types.h"',
        "",
        "#include <string_view>",
        "#include <utility>",
        "",
        '#include "base/json/json_reader.h"',
        '#include "base/json/json_writer.h"',
        '#include "base/values.h"',
        "",
        "namespace clawbrowser {",
        "",
        "namespace {",
        "",
        "base::expected<std::string, std::string> RequireString(",
        "    const base::DictValue& dict,",
        "    std::string_view key) {",
        "  const std::string* value = dict.FindString(key);",
        "  if (!value) {",
        '    return base::unexpected(std::string("missing required string field: ") + std::string(key));',
        "  }",
        "  return base::ok(*value);",
        "}",
        "",
        "base::expected<int, std::string> RequireInt(",
        "    const base::DictValue& dict,",
        "    std::string_view key) {",
        "  std::optional<int> value = dict.FindInt(key);",
        "  if (!value.has_value()) {",
        '    return base::unexpected(std::string("missing required int field: ") + std::string(key));',
        "  }",
        "  return base::ok(*value);",
        "}",
        "",
        "base::expected<bool, std::string> RequireBool(",
        "    const base::DictValue& dict,",
        "    std::string_view key) {",
        "  std::optional<bool> value = dict.FindBool(key);",
        "  if (!value.has_value()) {",
        '    return base::unexpected(std::string("missing required bool field: ") + std::string(key));',
        "  }",
        "  return base::ok(*value);",
        "}",
        "",
        "base::expected<double, std::string> RequireDouble(",
        "    const base::DictValue& dict,",
        "    std::string_view key) {",
        "  const base::Value* value = dict.Find(key);",
        "  if (!value) {",
        '    return base::unexpected(std::string("missing required number field: ") + std::string(key));',
        "  }",
        "  if (value->is_double()) return base::ok(value->GetDouble());",
        "  if (value->is_int()) return base::ok(static_cast<double>(value->GetInt()));",
        '  return base::unexpected(std::string("field is not a number: ") + std::string(key));',
        "}",
        "",
        "std::optional<double> FindOptionalDouble(",
        "    const base::DictValue& dict,",
        "    std::string_view key) {",
        "  const base::Value* value = dict.Find(key);",
        "  if (!value) return std::nullopt;",
        "  if (value->is_double()) return value->GetDouble();",
        "  if (value->is_int()) return static_cast<double>(value->GetInt());",
        "  return std::nullopt;",
        "}",
        "",
        "base::expected<const base::DictValue*, std::string> RequireDict(",
        "    const base::DictValue& dict,",
        "    std::string_view key) {",
        "  const base::DictValue* value = dict.FindDict(key);",
        "  if (!value) {",
        '    return base::unexpected(std::string("missing required object field: ") + std::string(key));',
        "  }",
        "  return base::ok(value);",
        "}",
        "",
        "base::expected<base::DictValue, std::string> ParseJsonToDict(",
        "    const std::string& json) {",
        "  auto parsed = base::JSONReader::ReadAndReturnValueWithError(",
        "      json, base::JSON_PARSE_RFC);",
        "  if (!parsed.has_value()) {",
        '    return base::unexpected("JSON parse error: " + parsed.error().message);',
        "  }",
        "  if (!parsed->is_dict()) {",
        '    return base::unexpected("expected a JSON object at root");',
        "  }",
        "  return base::ok(std::move(parsed->GetDict()));",
        "}",
        "",
        "std::string DictToJson(const base::DictValue& dict) {",
        "  std::string output;",
        "  base::JSONWriter::Write(base::Value(dict.Clone()), &output);",
        "  return output;",
        "}",
        "",
        "}  // namespace",
        "",
    ]

    for spec in specs:
        lines.extend(emit_special_member_definitions(spec))
        lines.extend(
            [
                f"base::expected<{spec.name}, std::string> {spec.name}::FromDict(",
                "    const base::DictValue& dict) {",
                f"  {spec.name} value;",
                "",
            ]
        )
        for field in spec.fields:
            parser_lines = emit_required_parse(field) if field.required else emit_optional_parse(field)
            lines.extend(parser_lines)
            lines.append("")
        lines.extend(
            [
                "  return base::ok(std::move(value));",
                "}",
                "",
                f"base::expected<{spec.name}, std::string> {spec.name}::FromJson(",
                "    const std::string& json) {",
                "  auto dict = ParseJsonToDict(json);",
                "  if (!dict.has_value()) return base::unexpected(dict.error());",
                "  return FromDict(*dict);",
                "}",
                "",
                f"base::DictValue {spec.name}::ToDict() const {{",
                "  base::DictValue dict;",
            ]
        )
        for field in spec.fields:
            lines.extend(emit_to_dict(field))
        lines.extend(
            [
                "  return dict;",
                "}",
                "",
                f"std::string {spec.name}::ToJson() const {{",
                "  return DictToJson(ToDict());",
                "}",
                "",
            ]
        )

    lines.extend(
        [
            "}  // namespace clawbrowser",
            "",
        ]
    )
    return "\n".join(lines)


def check_expected(path: pathlib.Path, content: str) -> bool:
    return path.exists() and path.read_text(encoding="utf-8").replace("\r\n", "\n") == content


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--schema", type=pathlib.Path, required=True)
    parser.add_argument("--header", type=pathlib.Path, required=True)
    parser.add_argument("--source", type=pathlib.Path, required=True)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--stamp", type=pathlib.Path)
    args = parser.parse_args()

    artifact = load_artifact(args.schema)
    specs = build_specs(artifact)
    header_content = emit_header(specs)
    source_content = emit_source(specs)

    if args.check:
        ok = True
        if not check_expected(args.header, header_content):
            print(f"STALE: {args.header} is not up to date with {args.schema}", file=sys.stderr)
            ok = False
        if not check_expected(args.source, source_content):
            print(f"STALE: {args.source} is not up to date with {args.schema}", file=sys.stderr)
            ok = False
        if not ok:
            sys.exit(1)
    else:
        args.header.parent.mkdir(parents=True, exist_ok=True)
        args.source.parent.mkdir(parents=True, exist_ok=True)
        with args.header.open("w", encoding="utf-8", newline="\n") as header:
            header.write(header_content)
        with args.source.open("w", encoding="utf-8", newline="\n") as source:
            source.write(source_content)

    if args.stamp:
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text("ok\n", encoding="utf-8")


if __name__ == "__main__":
    main()
