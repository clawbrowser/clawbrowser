#ifndef CLAWBROWSER_CLI_PROFILE_MANAGER_H_
#define CLAWBROWSER_CLI_PROFILE_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "clawbrowser/profile_envelope.h"

namespace clawbrowser {

struct ProfileInfo {
  std::string id;
  std::string created_at;
  std::string country;
};

// Manages fingerprint profiles on disk.
// Root: ~/.config/clawbrowser/ (or overridden for testing).
class ProfileManager {
 public:
  explicit ProfileManager(const base::FilePath& root_dir);
  ~ProfileManager();

  // Resolve API key: env var CLAWBROWSER_API_KEY first, then config.json.
  std::optional<std::string> ResolveApiKey();

  // Resolve API base URL: env var CLAWBROWSER_API_BASE_URL, config.json, or
  // default.
  std::string ResolveBaseUrl();

  // List all cached fingerprint profiles.
  std::vector<ProfileInfo> ListProfiles();

  // Path to fingerprint.json for a given profile ID.
  base::FilePath GetFingerprintPath(const std::string& id);

  // User-data-dir for a fingerprint profile.
  base::FilePath GetUserDataDir(const std::string& id);

  // User-data-dir for vanilla mode.
  base::FilePath GetVanillaUserDataDir();

  // Save a profile envelope to disk.
  base::expected<void, std::string> SaveProfile(
      const std::string& id,
      const ProfileEnvelope& envelope);

  // Read a cached profile from disk.
  base::expected<ProfileEnvelope, std::string> ReadProfile(
      const std::string& id);

  // Check if a cached profile exists.
  bool HasCachedProfile(const std::string& id);

 private:
  base::FilePath root_dir_;

  static constexpr char kDefaultBaseUrl[] = "https://api.clawbrowser.ai";
};

}  // namespace clawbrowser

#endif  // CLAWBROWSER_CLI_PROFILE_MANAGER_H_
