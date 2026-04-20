#ifndef CLAWBROWSER_CLI_PROFILE_MANAGER_H_
#define CLAWBROWSER_CLI_PROFILE_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "clawbrowser/profile_envelope.h"

namespace clawbrowser {

// Build-time default API base URL. Present for packaged official builds, absent
// for local/dev builds.
std::optional<std::string> GetBuildDefaultApiBaseUrl();

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

  // Resolve API base URL: env var CLAWBROWSER_API_BASE_URL, then config.json.
  std::optional<std::string> ResolveBaseUrl();

  // Persist API key into config.json while preserving unrelated config keys.
  base::expected<void, std::string> SaveApiKey(const std::string& api_key);

  // List all cached fingerprint profiles.
  std::vector<ProfileInfo> ListProfiles();

  // Best cached profile for implicit launch when no profile was requested.
  std::optional<std::string> FindBestCachedProfileId();

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
};

}  // namespace clawbrowser

#endif  // CLAWBROWSER_CLI_PROFILE_MANAGER_H_
