#ifndef CLAWBROWSER_PROXY_PROXY_AUTH_PRELOADER_H_
#define CLAWBROWSER_PROXY_PROXY_AUTH_PRELOADER_H_

#include <optional>

#include "clawbrowser/fingerprint_accessor.h"
#include "net/base/auth.h"

namespace content {
class BrowserContext;
}

namespace clawbrowser {

struct ProxyAuthPreloadConfig {
  net::AuthChallengeInfo challenge;
  net::AuthCredentials credentials;
};

std::optional<ProxyAuthPreloadConfig> BuildProxyAuthPreloadConfig(
    const RuntimeProxyConfig& proxy);

// Prepopulate Clawbrowser's profile HTTP auth cache for the active proxy.
void PreloadProxyAuth(content::BrowserContext* browser_context);

}  // namespace clawbrowser

#endif  // CLAWBROWSER_PROXY_PROXY_AUTH_PRELOADER_H_
