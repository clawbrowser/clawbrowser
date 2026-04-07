#include "clawbrowser/cli/profile_manager.h"

#include "base/environment.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"

namespace clawbrowser {

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

std::string ProfileManager::ResolveBaseUrl() {
  auto env = base::Environment::Create();
  if (std::optional<std::string> env_url =
          env->GetVar("CLAWBROWSER_API_BASE_URL");
      env_url.has_value() && !env_url->empty()) {
    return *env_url;
  }

  base::FilePath config_path = root_dir_.AppendASCII("config.json");
  std::string config_json;
  if (base::ReadFileToString(config_path, &config_json)) {
    auto parsed = base::JSONReader::Read(config_json, base::JSON_PARSE_RFC);
    if (parsed && parsed->is_dict()) {
      const std::string* url =
          parsed->GetDict().FindString("api_base_url");
      if (url && !url->empty())
        return *url;
    }
  }
  return kDefaultBaseUrl;
}

std::vector<ProfileInfo> ProfileManager::ListProfiles() {
  std::vector<ProfileInfo> profiles;
  base::FilePath browser_dir = root_dir_.AppendASCII("Browser");

  if (!base::DirectoryExists(browser_dir))
    return profiles;

  base::FileEnumerator enumerator(browser_dir, false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath dir = enumerator.Next(); !dir.empty();
       dir = enumerator.Next()) {
    std::string name = dir.BaseName().AsUTF8Unsafe();
    // Skip non-fingerprint directories (e.g., "Default" for vanilla)
    if (!name.starts_with("fp_"))
      continue;

    base::FilePath fp_path = dir.AppendASCII("fingerprint.json");
    if (!base::PathExists(fp_path))
      continue;

    ProfileInfo info;
    info.id = name;

    // Try to read metadata
    std::string json;
    if (base::ReadFileToString(fp_path, &json)) {
      auto envelope = ProfileEnvelope::Parse(json);
      if (envelope.has_value()) {
        info.created_at = envelope->created_at;
        info.country = envelope->request.country;
      }
    }

    profiles.push_back(std::move(info));
  }

  return profiles;
}

base::FilePath ProfileManager::GetFingerprintPath(const std::string& id) {
  return root_dir_.AppendASCII("Browser")
      .AppendASCII(id)
      .AppendASCII("fingerprint.json");
}

base::FilePath ProfileManager::GetUserDataDir(const std::string& id) {
  return root_dir_.AppendASCII("Browser").AppendASCII(id);
}

base::FilePath ProfileManager::GetVanillaUserDataDir() {
  return root_dir_.AppendASCII("Browser").AppendASCII("Default");
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
  std::string json = envelope.Serialize();
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
  return ProfileEnvelope::Parse(json);
}

bool ProfileManager::HasCachedProfile(const std::string& id) {
  return base::PathExists(GetFingerprintPath(id));
}

}  // namespace clawbrowser
