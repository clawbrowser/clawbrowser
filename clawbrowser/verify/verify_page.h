#ifndef CLAWBROWSER_VERIFY_VERIFY_PAGE_H_
#define CLAWBROWSER_VERIFY_VERIFY_PAGE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

namespace network {
class SimpleURLLoader;
}

namespace clawbrowser {

class ApiClient;
struct ClientProxyGeo;

// Returns no result for malformed/non-public observations; callers may try the
// other fixed provider. A valid but changed exit is a definitive failed check.
std::optional<base::DictValue> ClientProxyGeoVerificationResult(
    const ClientProxyGeo& expected, const base::DictValue& observed);
bool ProxyHostRequiresClientGeography(const std::string& host);

bool VerifyFailureExitEnabledForCommandLine(
    const base::CommandLine& command_line);

// Returns the active managed-proxy privacy contract supported by this
// process. Version 2 means that a fingerprint-backed proxy was loaded and the
// browser was launched through both the fail-closed proxy and WebRTC privacy
// contracts.
int ManagedProxyPrivacyCapabilityForCommandLine(
    const base::CommandLine& command_line,
    bool fingerprint_proxy_loaded);

// WebUI controller for clawbrowser://verify page.
// Injects expected fingerprint values and handles proxy verification.
class VerifyPageUI : public content::WebUIController {
 public:
  explicit VerifyPageUI(content::WebUI* web_ui);
  ~VerifyPageUI() override;

 private:
  // Handle "verifyProxy" message from page JS.
  void HandleVerifyProxy(const base::ListValue& args);
  void VerifyClientProxyGeo(ClientProxyGeo expected, size_t provider);

  // Handle "verifyComplete" message — result of all checks.
  void HandleVerifyComplete(const base::ListValue& args);

  // Configure data source: add resources and inject expected values.
  void SetupDataSource(content::WebUIDataSource* source);

  // 30s timeout: if verify fails and no CDP client connects, exit(1).
  void StartFailureExitTimer();
  void OnFailureExitTimeout();

  bool automation_mode_ = false;
  bool verify_passed_ = false;
  bool verification_complete_ = false;
  base::OneShotTimer failure_exit_timer_;
  std::unique_ptr<ApiClient> api_client_;
  std::unique_ptr<network::SimpleURLLoader> client_proxy_loader_;
  base::WeakPtrFactory<VerifyPageUI> weak_ptr_factory_{this};
};

// URL host for the verify page.
inline constexpr char kVerifyHost[] = "verify";

}  // namespace clawbrowser

#endif  // CLAWBROWSER_VERIFY_VERIFY_PAGE_H_
