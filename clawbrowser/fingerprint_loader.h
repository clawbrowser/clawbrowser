#ifndef CLAWBROWSER_FINGERPRINT_LOADER_H_
#define CLAWBROWSER_FINGERPRINT_LOADER_H_

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/types/expected.h"

namespace clawbrowser {

struct RuntimeProxyConfig;

// Command-line switch for fingerprint file path.
inline constexpr char kFingerprintPathSwitch[] = "clawbrowser-fp-path";
inline constexpr char kFingerprintChildDataSwitch[] =
    "clawbrowser-fp-child-data";

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

// Load fingerprint if --clawbrowser-fp-path is present on command line.
// Returns success even if the switch is absent (vanilla mode).
base::expected<void, std::string> LoadFingerprintFromCommandLine(
    const base::CommandLine& command_line);

}  // namespace clawbrowser

#endif  // CLAWBROWSER_FINGERPRINT_LOADER_H_
