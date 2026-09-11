#include "clawbrowser/font_catalog.h"

#include <set>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "crypto/hash.h"

namespace clawbrowser {
namespace {
std::string Digest(std::string_view bytes) {
  return base::ToLowerASCII(base::HexEncode(crypto::hash::Sha256(bytes)));
}

bool SafeFontName(std::string_view name) {
  if (name.empty() || name.front() == '.' ||
      name.find("..") != std::string_view::npos) {
    return false;
  }
  for (char c : name) {
    if (!base::IsAsciiAlphaNumeric(c) && c != '-' && c != '_' && c != '.')
      return false;
  }
  return base::EndsWith(name, ".ttf") || base::EndsWith(name, ".otf") ||
         base::EndsWith(name, ".ttc");
}
}  // namespace

base::expected<std::string, std::string> ValidateFontCatalog(
    const base::FilePath& catalog_dir,
    std::string_view expected_manifest_sha256) {
  std::string manifest;
  if (expected_manifest_sha256.size() != 64 ||
      !base::ReadFileToStringWithMaxSize(
          catalog_dir.AppendASCII("manifest.json"), &manifest, 1024 * 1024) ||
      Digest(manifest) != expected_manifest_sha256) {
    return base::unexpected("font catalog manifest missing or checksum mismatch");
  }
  auto parsed = base::JSONReader::Read(manifest, base::JSON_PARSE_RFC);
  if (!parsed || !parsed->is_dict())
    return base::unexpected("invalid font catalog manifest");
  const auto& dict = parsed->GetDict();
  const auto* id = dict.FindString("catalog_id");
  const auto* fonts = dict.FindList("fonts");
  if (!id || id->empty() || !fonts || fonts->empty())
    return base::unexpected("empty font catalog");

  std::set<std::string> expected_files;
  const auto font_dir = catalog_dir.AppendASCII("fonts");
  for (const auto& entry : *fonts) {
    if (!entry.is_dict())
      return base::unexpected("invalid font entry");
    const auto* name = entry.GetDict().FindString("file");
    const auto* checksum = entry.GetDict().FindString("sha256");
    if (!name || !SafeFontName(*name) || !checksum || checksum->size() != 64 ||
        !expected_files.insert(*name).second) {
      return base::unexpected("invalid or duplicate font entry");
    }
    std::string bytes;
    const auto path = font_dir.AppendASCII(*name);
    if (base::IsLink(path) ||
        !base::ReadFileToStringWithMaxSize(path, &bytes, 128 * 1024 * 1024) ||
        Digest(bytes) != *checksum) {
      return base::unexpected("font asset missing or checksum mismatch: " + *name);
    }
  }
  // Fontconfig scans the directory, not merely manifest entries. Reject extra
  // files/directories rather than accidentally exposing an unpinned family.
  base::FileEnumerator files(font_dir, false,
                            base::FileEnumerator::FILES |
                                base::FileEnumerator::DIRECTORIES);
  size_t actual_count = 0;
  for (auto path = files.Next(); !path.empty(); path = files.Next()) {
    if (files.GetInfo().IsDirectory() ||
        !expected_files.contains(path.BaseName().AsUTF8Unsafe())) {
      return base::unexpected("unlisted file in font catalog");
    }
    ++actual_count;
  }
  if (actual_count != expected_files.size())
    return base::unexpected("incomplete font catalog");
  return base::ok(*id);
}
}  // namespace clawbrowser
