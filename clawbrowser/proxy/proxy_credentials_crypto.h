#ifndef CLAWBROWSER_PROXY_PROXY_CREDENTIALS_CRYPTO_H_
#define CLAWBROWSER_PROXY_PROXY_CREDENTIALS_CRYPTO_H_

#include <string>

#include "base/types/expected.h"

namespace clawbrowser {

struct ProxyCredentialsSecret {
  std::string username;
  std::string password;
};

base::expected<std::string, std::string> EncryptProxyCredentials(
    const std::string& username,
    const std::string& password);

base::expected<ProxyCredentialsSecret, std::string> DecryptProxyCredentials(
    const std::string& encrypted_blob);

}  // namespace clawbrowser

#endif  // CLAWBROWSER_PROXY_PROXY_CREDENTIALS_CRYPTO_H_
