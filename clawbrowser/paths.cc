#include "clawbrowser/paths.h"

#include <optional>
#include <string>

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/path_service.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#endif

namespace clawbrowser {

base::FilePath GetClawbrowserConfigDir() {
  auto env = base::Environment::Create();
  if (std::optional<std::string> override =
          env->GetVar("CLAWBROWSER_CONFIG_DIR");
      override.has_value() && !override->empty()) {
    return base::FilePath::FromUTF8Unsafe(*override);
  }

#if BUILDFLAG(IS_WIN)
  base::FilePath local_app_data;
  if (base::PathService::Get(base::DIR_LOCAL_APP_DATA, &local_app_data) &&
      !local_app_data.empty()) {
    return local_app_data.AppendASCII("Clawbrowser");
  }
#endif

  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);
  return home_dir.AppendASCII(".config").AppendASCII("clawbrowser");
}

}  // namespace clawbrowser
