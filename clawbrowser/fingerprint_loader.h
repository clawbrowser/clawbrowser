#ifndef CLAWBROWSER_FINGERPRINT_LOADER_H_
#define CLAWBROWSER_FINGERPRINT_LOADER_H_

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/types/expected.h"

namespace clawbrowser {

// Decides whether a surface should be spoofed, combining the fingerprint's
// declared intent with the local command-line switches.
//
// surface_policy is the default: a profile asking for "override" gets spoofing
// without any extra flag. The switches remain an explicit local override --
// --enable-* forces a surface on, --disable-* wins over everything.
//
// This has to be resolved identically in the browser process and in every
// child. Child processes receive only the raw switches (patch 004 propagates
// them) and rebuild their own RuntimeFingerprint from the profile JSON, which
// carries no resolved spoofing state -- so resolving once in the browser is not
// enough. The renderer is where the canvas/WebGL patches actually run.
inline bool ResolveSurfaceSpoofing(bool forced_on,
                                   bool forced_off,
                                   bool policy_requests_override) {
  if (forced_off) {
    return false;
  }
  return forced_on || policy_requests_override;
}

struct RuntimeProxyConfig;

// Command-line switch for fingerprint file path.
inline constexpr char kFingerprintPathSwitch[] = "clawbrowser-fp-path";
inline constexpr char kFingerprintChildDataSwitch[] =
    "clawbrowser-fp-child-data";
// Marks renderer/GPU command lines that must not continue without a loaded
// fingerprint. This keeps an accidentally missing/corrupt payload from turning
// a managed profile into a native-fingerprint child process.
inline constexpr char kRequireFingerprintSwitch[] =
    "clawbrowser-require-fingerprint";

// Load fingerprint from file and store in FingerprintAccessor.
// Must be called before sandbox lockdown in renderer/GPU processes.
base::expected<void, std::string> LoadFingerprint(
    const base::FilePath& path);

// Build a base64-encoded child-safe payload from an on-disk profile envelope.
// The payload carries renderer/GPU-visible fingerprint state plus proxy config
// needed by child-process spoofing and leak-prevention patches.
base::expected<std::string, std::string> BuildChildFingerprintPayload(
    const base::FilePath& path);
base::expected<std::string, std::string> BuildChildFingerprintPayload(
    const base::FilePath& path,
    const RuntimeProxyConfig& proxy_override);

// Load fingerprint from the managed child payload or profile path. Returns
// success with no data only for vanilla command lines; a command carrying
// --clawbrowser-require-fingerprint fails if neither source is present.
base::expected<void, std::string> LoadFingerprintFromCommandLine(
    const base::CommandLine& command_line);

}  // namespace clawbrowser

#endif  // CLAWBROWSER_FINGERPRINT_LOADER_H_
