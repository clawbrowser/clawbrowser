#include "clawbrowser/proxy/proxy_auth_preloader.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace clawbrowser {

namespace {

constexpr char kBasicAuthScheme[] = "basic";
constexpr char kProxyAuthRealm[] = "clawbrowser";
constexpr char kProxyAuthChallenge[] = "Basic realm=\"clawbrowser\"";
constexpr char kProxyAuthPath[] = "/";

bool HasProxyAuthFields(const RuntimeProxyConfig& proxy) {
  return proxy.host.has_value() && !proxy.host->empty() &&
         proxy.port.has_value() && proxy.username.has_value() &&
         !proxy.username->empty() && proxy.password.has_value() &&
         !proxy.password->empty();
}

std::optional<std::string> NormalizedProxyScheme(
    const RuntimeProxyConfig& proxy) {
  const std::string scheme =
      proxy.scheme.has_value() && !proxy.scheme->empty() ? *proxy.scheme
                                                         : "https";
  if (scheme == "http" || scheme == "https" || scheme == "socks5") {
    return scheme;
  }
  return std::nullopt;
}

GURL BuildProxyUrl(const RuntimeProxyConfig& proxy) {
  std::optional<std::string> scheme = NormalizedProxyScheme(proxy);
  if (!scheme.has_value()) {
    return GURL();
  }

  return GURL(*scheme + "://" + *proxy.host + ":" +
              base::NumberToString(*proxy.port));
}

}  // namespace

std::optional<ProxyAuthPreloadConfig> BuildProxyAuthPreloadConfig(
    const RuntimeProxyConfig& proxy) {
  if (!HasProxyAuthFields(proxy)) {
    return std::nullopt;
  }

  url::SchemeHostPort challenger(BuildProxyUrl(proxy));
  if (!challenger.IsValid()) {
    return std::nullopt;
  }

  ProxyAuthPreloadConfig config;
  config.challenge.is_proxy = true;
  config.challenge.challenger = challenger;
  config.challenge.scheme = kBasicAuthScheme;
  config.challenge.realm = kProxyAuthRealm;
  config.challenge.challenge = kProxyAuthChallenge;
  config.challenge.path = kProxyAuthPath;
  config.credentials = net::AuthCredentials(
      base::UTF8ToUTF16(*proxy.username),
      base::UTF8ToUTF16(*proxy.password));
  return config;
}

void PreloadProxyAuth(content::BrowserContext* browser_context) {
  if (!browser_context) {
    return;
  }

  const RuntimeProxyConfig* proxy = FingerprintAccessor::GetProxy();
  if (!proxy) {
    return;
  }

  std::optional<ProxyAuthPreloadConfig> config =
      BuildProxyAuthPreloadConfig(*proxy);
  if (!config) {
    return;
  }

  content::StoragePartition* storage_partition =
      browser_context->GetDefaultStoragePartition();
  storage_partition->GetNetworkContext()->AddAuthCacheEntry(
      config->challenge, net::NetworkAnonymizationKey(), config->credentials,
      base::DoNothing());
}

}  // namespace clawbrowser
