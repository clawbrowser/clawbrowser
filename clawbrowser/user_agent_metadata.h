#ifndef CLAWBROWSER_USER_AGENT_METADATA_H_
#define CLAWBROWSER_USER_AGENT_METADATA_H_

#include <string>
#include <vector>

#include "clawbrowser/fingerprint_accessor.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace clawbrowser {

namespace internal {

inline std::string ExtractDelimitedToken(const std::string& input,
                                         const std::string& marker,
                                         bool allow_underscore) {
  size_t start = input.find(marker);
  if (start == std::string::npos) {
    return std::string();
  }
  start += marker.size();

  size_t end = start;
  while (end < input.size()) {
    const char c = input[end];
    if ((c >= '0' && c <= '9') || c == '.' ||
        (allow_underscore && c == '_')) {
      ++end;
      continue;
    }
    break;
  }

  return input.substr(start, end - start);
}

inline std::string RuntimeFullVersion(const RuntimeFingerprint& fingerprint) {
  if (fingerprint.user_agent_data.ua_full_version.has_value() &&
      !fingerprint.user_agent_data.ua_full_version->empty()) {
    return *fingerprint.user_agent_data.ua_full_version;
  }
  if (!fingerprint.browser_version.empty()) {
    return fingerprint.browser_version;
  }
  return ExtractDelimitedToken(fingerprint.user_agent, "Chrome/", false);
}

inline void AppendBrands(
    const std::vector<RuntimeClientHintBrand>& source,
    std::vector<blink::UserAgentBrandVersion>* destination) {
  for (const auto& brand : source) {
    if (brand.brand.empty() || brand.version.empty()) {
      continue;
    }
    destination->emplace_back(brand.brand, brand.version);
  }
}

}  // namespace internal

inline blink::UserAgentMetadata BuildUserAgentMetadata(
    const RuntimeFingerprint& fingerprint) {
  blink::UserAgentMetadata metadata;

  internal::AppendBrands(fingerprint.user_agent_data.brands,
                         &metadata.brand_version_list);
  internal::AppendBrands(fingerprint.user_agent_data.full_version_list,
                         &metadata.brand_full_version_list);
  metadata.full_version = internal::RuntimeFullVersion(fingerprint);
  metadata.platform = fingerprint.user_agent_data.platform.empty()
                          ? fingerprint.platform
                          : fingerprint.user_agent_data.platform;
  metadata.platform_version =
      fingerprint.user_agent_data.platform_version.empty()
          ? fingerprint.os_version
          : fingerprint.user_agent_data.platform_version;
  metadata.architecture = fingerprint.user_agent_data.architecture.empty()
                              ? fingerprint.architecture
                              : fingerprint.user_agent_data.architecture;
  metadata.model = fingerprint.user_agent_data.model;
  metadata.mobile = fingerprint.user_agent_data.mobile;
  metadata.bitness = fingerprint.user_agent_data.bitness;
  metadata.wow64 = false;
  return metadata;
}

}  // namespace clawbrowser

#endif  // CLAWBROWSER_USER_AGENT_METADATA_H_
