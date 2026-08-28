#ifndef CLAWBROWSER_LOGGING_H_
#define CLAWBROWSER_LOGGING_H_

#include "base/logging.h"

namespace clawbrowser {

// Set by startup orchestration based on --verbose flag.
void SetVerbose(bool verbose);
bool IsVerbose();

}  // namespace clawbrowser

// Clawbrowser logging macros — only emit when --verbose is set.
// Errors always emit (regardless of --verbose).
#define CLAW_LOG(severity) \
  if (severity == logging::LOGGING_ERROR || clawbrowser::IsVerbose()) \
    LOG(severity) << "[clawbrowser] "

#define CLAW_VLOG() \
  if (clawbrowser::IsVerbose()) \
    LOG(INFO) << "[clawbrowser] "

#endif  // CLAWBROWSER_LOGGING_H_
