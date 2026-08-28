#ifndef CLAWBROWSER_PROXY_PROXY_AUTH_LOGIN_DELEGATE_H_
#define CLAWBROWSER_PROXY_PROXY_AUTH_LOGIN_DELEGATE_H_

#include "content/public/browser/login_delegate.h"

namespace content {
class WebContents;
}

namespace clawbrowser {

// Automatically responds to proxy 407 challenges with stored credentials.
// Created by ContentBrowserClient::CreateLoginDelegate() when
// the challenge is for a proxy and fingerprint mode is active.
class ProxyAuthLoginDelegate : public content::LoginDelegate {
 public:
  ProxyAuthLoginDelegate(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      LoginAuthRequiredCallback auth_required_callback);
  ~ProxyAuthLoginDelegate() override;
};

}  // namespace clawbrowser

#endif  // CLAWBROWSER_PROXY_PROXY_AUTH_LOGIN_DELEGATE_H_
