#ifndef CLAWBROWSER_STARTUP_H_
#define CLAWBROWSER_STARTUP_H_

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace clawbrowser {

// Result of startup orchestration.
struct StartupResult {
  // True when startup was fully handled or a managed launch was blocked.
  bool should_exit = false;
  int exit_code = 0;
};

// Handle CLI-only commands that should exit before full browser startup.
// Returns an exit code on handled commands, or std::nullopt to continue.
base::expected<std::optional<int>, std::string> HandleBasicStartupComplete(
    const base::CommandLine& command_line);

// Run the full clawbrowser startup sequence:
// 1. Parse CLI args
// 2. Handle --list (print and exit)
// 3. If --fingerprint:
//    a. Resolve API key
//    b. Check cache (or --regenerate)
//    c. Call API if needed, save profile
//    d. Load fingerprint into accessor
//    e. Set --clawbrowser-fp-path, --user-data-dir, --proxy-server,
//       --lang, --accept-lang on command line for child-process loading
// 4. If no --fingerprint: vanilla mode, set Default user-data-dir
//
// Managed fingerprint acquisition, persistence, and load errors fail closed:
// they print to stderr (and JSON to stdout if --output=json) and return a
// non-zero exit result instead of continuing with native browser surfaces.
// Returns StartupResult indicating whether to continue or exit.
base::expected<StartupResult, std::string> RunStartup(
    base::CommandLine* command_line,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

// Run the command-line-only startup phase before Clawbrowser resolves the user
// data directory and initializes browser/network services.
base::expected<StartupResult, std::string> ConfigureEarlyStartup(
    base::CommandLine* command_line);

// Configure command-line switches that Chromium consumes before
// chrome::DIR_USER_DATA is initialized. This must run before Chromium's
// InitializeUserDataDir(), otherwise a fresh launch can bind to the default
// Chromium profile before Clawbrowser selects the fingerprint/auth profile.
void ConfigureCommandLineBeforeUserDataDir(base::CommandLine* command_line);

}  // namespace clawbrowser

#endif  // CLAWBROWSER_STARTUP_H_
