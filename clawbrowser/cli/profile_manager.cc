#include "clawbrowser/cli/profile_manager.h"

#include <algorithm>
#include <string_view>

#include "base/environment.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"

namespace clawbrowser {
namespace {

constexpr char kEncodedProfileDirPrefix[] = "id_";

base::FilePath GetBrowserDir(const base::FilePath& root_dir) {
  return root_dir.AppendASCII("Browser");
}

char HexDigitForNibble(unsigned char nibble) {
  return nibble < 10 ? static_cast<char>('0' + nibble)
                     : static_cast<char>('a' + (nibble - 10));
}

std::string EncodeProfileId(std::string_view id) {
  std::string encoded = kEncodedProfileDirPrefix;
  encoded.reserve(encoded.size() + id.size() * 2);
  for (unsigned char ch : id) {
    encoded.push_back(HexDigitForNibble(ch >> 4));
    encoded.push_back(HexDigitForNibble(ch & 0x0f));
  }
  return encoded;
}

std::optional<base::FilePath> GetLegacyProfileDir(const base::FilePath& root_dir,
                                                  const std::string& id) {
  base::FilePath relative_path = base::FilePath::FromUTF8Unsafe(id);
  if (relative_path.empty() || relative_path.IsAbsolute() ||
      relative_path.ReferencesParent()) {
    return std::nullopt;
  }
  return GetBrowserDir(root_dir).Append(relative_path);
}

base::FilePath GetEncodedProfileDir(const base::FilePath& root_dir,
                                    const std::string& id) {
  return GetBrowserDir(root_dir).AppendASCII(EncodeProfileId(id));
}

base::FilePath ResolveProfileDir(const base::FilePath& root_dir,
                                 const std::string& id) {
  if (auto legacy_dir = GetLegacyProfileDir(root_dir, id);
      legacy_dir.has_value() && base::DirectoryExists(*legacy_dir)) {
    return *legacy_dir;
  }
  return GetEncodedProfileDir(root_dir, id);
}

base::FilePath ResolveFingerprintPath(const base::FilePath& root_dir,
                                      const std::string& id) {
  if (auto legacy_dir = GetLegacyProfileDir(root_dir, id);
      legacy_dir.has_value()) {
    base::FilePath legacy_path = legacy_dir->AppendASCII("fingerprint.json");
    if (base::PathExists(legacy_path)) {
      return legacy_path;
    }
  }
  return GetEncodedProfileDir(root_dir, id).AppendASCII("fingerprint.json");
}

std::string InferProfileIdFromProfileDir(const base::FilePath& browser_dir,
                                         const base::FilePath& profile_dir) {
  if (profile_dir.DirName() == browser_dir) {
    return profile_dir.BaseName().AsUTF8Unsafe();
  }

  base::FilePath relative_path;
  if (browser_dir.AppendRelativePath(profile_dir, &relative_path)) {
    return relative_path.AsUTF8Unsafe();
  }
  return profile_dir.BaseName().AsUTF8Unsafe();
}

}  // namespace

std::optional<std::string> GetBuildDefaultApiBaseUrl() {
#if defined(CLAWBROWSER_DEFAULT_API_BASE_URL)
  constexpr std::string_view kDefaultApiBaseUrl =
      CLAWBROWSER_DEFAULT_API_BASE_URL;
  return std::string(kDefaultApiBaseUrl);
#else
  return std::nullopt;
#endif
}

ProfileManager::ProfileManager(const base::FilePath& root_dir)
    : root_dir_(root_dir) {}

ProfileManager::~ProfileManager() = default;

std::optional<std::string> ProfileManager::ResolveApiKey() {
  // 1. Environment variable
  auto env = base::Environment::Create();
  if (std::optional<std::string> env_key =
          env->GetVar("CLAWBROWSER_API_KEY");
      env_key.has_value() && !env_key->empty()) {
    return env_key;
  }

  // 2. config.json
  base::FilePath config_path = root_dir_.AppendASCII("config.json");
  std::string config_json;
  if (base::ReadFileToString(config_path, &config_json)) {
    auto parsed = base::JSONReader::Read(config_json, base::JSON_PARSE_RFC);
    if (parsed && parsed->is_dict()) {
      const std::string* key = parsed->GetDict().FindString("api_key");
      if (key && !key->empty())
        return *key;
    }
  }

  return std::nullopt;
}

std::optional<std::string> ProfileManager::ResolveBaseUrl() {
  auto env = base::Environment::Create();
  if (std::optional<std::string> env_url =
          env->GetVar("CLAWBROWSER_API_BASE_URL");
      env_url.has_value() && !env_url->empty()) {
    return env_url;
  }

  base::FilePath config_path = root_dir_.AppendASCII("config.json");
  std::string config_json;
  if (base::ReadFileToString(config_path, &config_json)) {
    auto parsed = base::JSONReader::Read(config_json, base::JSON_PARSE_RFC);
    if (parsed && parsed->is_dict()) {
      const std::string* url = parsed->GetDict().FindString("api_base_url");
      if (url && !url->empty())
        return *url;
    }
  }

  return GetBuildDefaultApiBaseUrl();
}

base::expected<void, std::string> ProfileManager::SaveApiKey(
    const std::string& api_key) {
  if (api_key.empty()) {
    return base::unexpected("api key cannot be empty");
  }

  if (!base::CreateDirectory(root_dir_)) {
    return base::unexpected(
        "cannot create config directory " + root_dir_.AsUTF8Unsafe());
  }

  base::DictValue config_dict;
  base::FilePath config_path = root_dir_.AppendASCII("config.json");
  std::string config_json;
  if (base::ReadFileToString(config_path, &config_json)) {
    auto parsed = base::JSONReader::Read(config_json, base::JSON_PARSE_RFC);
    if (parsed && parsed->is_dict()) {
      config_dict = parsed->GetDict().Clone();
    }
  }

  config_dict.Set("api_key", api_key);

  std::string output_json;
  base::JSONWriter::Write(base::Value(std::move(config_dict)), &output_json);
  if (!base::WriteFile(config_path, output_json)) {
    return base::unexpected(
        "cannot write to " + config_path.AsUTF8Unsafe());
  }

  return base::ok();
}

std::vector<ProfileInfo> ProfileManager::ListProfiles() {
  std::vector<ProfileInfo> profiles;
  base::FilePath browser_dir = GetBrowserDir(root_dir_);

  if (!base::DirectoryExists(browser_dir))
    return profiles;

  base::FileEnumerator enumerator(browser_dir, true,
                                  base::FileEnumerator::FILES);
  for (base::FilePath fp_path = enumerator.Next(); !fp_path.empty();
       fp_path = enumerator.Next()) {
    if (fp_path.BaseName().AsUTF8Unsafe() != "fingerprint.json")
      continue;

    ProfileInfo info;
    info.id = InferProfileIdFromProfileDir(browser_dir, fp_path.DirName());

    // Try to read metadata
    std::string json;
    if (base::ReadFileToString(fp_path, &json)) {
      auto envelope = ProfileEnvelope::Parse(json);
      if (envelope.has_value()) {
        if (envelope->profile_id.has_value()) {
          info.id = *envelope->profile_id;
        }
        info.created_at = envelope->created_at;
        info.country = envelope->request.country;
      }
    }

    auto duplicate = std::find_if(
        profiles.begin(), profiles.end(),
        [&info](const ProfileInfo& existing) { return existing.id == info.id; });
    if (duplicate != profiles.end())
      continue;

    profiles.push_back(std::move(info));
  }

  return profiles;
}

std::optional<std::string> ProfileManager::FindBestCachedProfileId() {
  std::vector<ProfileInfo> profiles = ListProfiles();
  if (profiles.empty()) {
    return std::nullopt;
  }

  auto best = std::max_element(
      profiles.begin(), profiles.end(),
      [](const ProfileInfo& a, const ProfileInfo& b) {
        if (a.created_at != b.created_at) {
          return a.created_at < b.created_at;
        }
        return a.id > b.id;
      });
  return best->id;
}

base::FilePath ProfileManager::GetFingerprintPath(const std::string& id) {
  return ResolveFingerprintPath(root_dir_, id);
}

base::FilePath ProfileManager::GetUserDataDir(const std::string& id) {
  return ResolveProfileDir(root_dir_, id);
}

base::FilePath ProfileManager::GetVanillaUserDataDir() {
  return GetBrowserDir(root_dir_).AppendASCII("Default");
}

base::expected<void, std::string> ProfileManager::SaveProfile(
    const std::string& id,
    const ProfileEnvelope& envelope) {
  base::FilePath profile_dir = GetUserDataDir(id);
  if (!base::CreateDirectory(profile_dir)) {
    return base::unexpected(
        "cannot write to " + profile_dir.AsUTF8Unsafe());
  }

  base::FilePath fp_path = profile_dir.AppendASCII("fingerprint.json");
  ProfileEnvelope envelope_to_save = envelope;
  envelope_to_save.profile_id = id;
  std::string json = envelope_to_save.Serialize();
  if (!base::WriteFile(fp_path, json)) {
    return base::unexpected(
        "cannot write to " + fp_path.AsUTF8Unsafe());
  }

  return base::ok();
}

base::expected<ProfileEnvelope, std::string> ProfileManager::ReadProfile(
    const std::string& id) {
  base::FilePath fp_path = GetFingerprintPath(id);
  std::string json;
  if (!base::ReadFileToString(fp_path, &json)) {
    return base::unexpected(
        "failed to read " + fp_path.AsUTF8Unsafe());
  }
  auto envelope = ProfileEnvelope::Parse(json);
  if (!envelope.has_value()) {
    return base::unexpected(envelope.error());
  }
  if (!envelope->profile_id.has_value()) {
    envelope->profile_id = id;
  }
  return envelope;
}

bool ProfileManager::HasCachedProfile(const std::string& id) {
  return base::PathExists(GetFingerprintPath(id));
}

}  // namespace clawbrowser
