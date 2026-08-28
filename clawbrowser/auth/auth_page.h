#ifndef CLAWBROWSER_AUTH_AUTH_PAGE_H_
#define CLAWBROWSER_AUTH_AUTH_PAGE_H_

#include <string_view>

#include "base/command_line.h"
#include "base/values.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

namespace clawbrowser {

std::string_view GetAuthDashboardUrl();
std::string_view GetAuthRestartMessage();
base::CommandLine BuildAuthRelaunchCommandLine(
    const base::CommandLine& current_command_line);
bool IsValidApiKeyInput(std::string_view input);

class AuthPageUI : public content::WebUIController {
 public:
  explicit AuthPageUI(content::WebUI* web_ui);
  ~AuthPageUI() override;

 private:
  void HandleOpenDashboard(const base::ListValue& args);
  void HandleSaveApiKey(const base::ListValue& args);
  void HandleRestartAfterSave(const base::ListValue& args);
  void SetupDataSource(content::WebUIDataSource* source);
};

inline constexpr char kAuthHost[] = "auth";

}  // namespace clawbrowser

#endif  // CLAWBROWSER_AUTH_AUTH_PAGE_H_
