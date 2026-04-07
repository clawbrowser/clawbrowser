#ifndef CLAWBROWSER_STARTUP_H_
#define CLAWBROWSER_STARTUP_H_

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
  bool should_exit = false;  // True if --list was handled (exit after print)
  int exit_code = 0;
};

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
// Errors print to stderr (and JSON to stdout if --output=json).
// Returns StartupResult indicating whether to continue or exit.
base::expected<StartupResult, std::string> RunStartup(
    base::CommandLine* command_line,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

// Run the command-line-only startup phase before Chromium resolves the user
// data directory and initializes browser/network services.
base::expected<StartupResult, std::string> ConfigureEarlyStartup(
    base::CommandLine* command_line);

}  // namespace clawbrowser

#endif  // CLAWBROWSER_STARTUP_H_
