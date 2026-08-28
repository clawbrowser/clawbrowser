#include "clawbrowser/proxy/proxy_auth_login_delegate.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "clawbrowser/fingerprint_accessor.h"
#include "clawbrowser/proxy/proxy_config.h"
#include "net/base/auth.h"

namespace clawbrowser {

namespace {

void RunAuthRequiredCallback(
    content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback,
    std::optional<net::AuthCredentials> credentials) {
  std::move(auth_required_callback).Run(std::move(credentials));
}

}  // namespace

ProxyAuthLoginDelegate::ProxyAuthLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    LoginAuthRequiredCallback auth_required_callback) {
  std::optional<net::AuthCredentials> credentials;

  // Only auto-respond for proxy auth when fingerprint is active
  const auto* proxy = FingerprintAccessor::GetProxy();
  if (proxy && auth_info.is_proxy) {
    auto config = BuildClawbrowserProxyConfig(*proxy);
    if (config && !config->username.empty() && !config->password.empty()) {
      credentials.emplace(base::UTF8ToUTF16(config->username),
                          base::UTF8ToUTF16(config->password));
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunAuthRequiredCallback,
                     std::move(auth_required_callback),
                     std::move(credentials)));
}

ProxyAuthLoginDelegate::~ProxyAuthLoginDelegate() = default;

}  // namespace clawbrowser
