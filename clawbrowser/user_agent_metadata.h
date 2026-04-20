#ifndef CLAWBROWSER_USER_AGENT_METADATA_H_
#define CLAWBROWSER_USER_AGENT_METADATA_H_

#include <algorithm>
#include <string>

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

inline std::string ClawbrowserFullVersion(const RuntimeFingerprint& fingerprint) {
  std::string version =
      ExtractDelimitedToken(fingerprint.user_agent, "Clawbrowser/", false);
  if (!version.empty()) {
    return version;
  }
  return "0.0.0.0";
}

inline std::string ClawbrowserMajorVersion(const RuntimeFingerprint& fingerprint) {
  const std::string full_version = ClawbrowserFullVersion(fingerprint);
  const size_t dot = full_version.find('.');
  return dot == std::string::npos ? full_version
                                  : full_version.substr(0, dot);
}

inline std::string PlatformName(const RuntimeFingerprint& fingerprint) {
  if (fingerprint.platform == "MacIntel") {
    return "macOS";
  }
  return fingerprint.platform;
}

inline std::string PlatformVersion(const RuntimeFingerprint& fingerprint) {
  std::string version =
      ExtractDelimitedToken(fingerprint.user_agent, "Mac OS X ", true);
  std::replace(version.begin(), version.end(), '_', '.');
  return version;
}

}  // namespace internal

inline blink::UserAgentMetadata BuildUserAgentMetadata(
    const RuntimeFingerprint& fingerprint) {
  blink::UserAgentMetadata metadata;
  const std::string full_version =
      internal::ClawbrowserFullVersion(fingerprint);
  const std::string major_version =
      internal::ClawbrowserMajorVersion(fingerprint);

  metadata.brand_version_list.emplace_back("Clawbrowser", major_version);
  metadata.brand_full_version_list.emplace_back("Clawbrowser", full_version);
  metadata.full_version = full_version;
  metadata.platform = internal::PlatformName(fingerprint);
  metadata.platform_version = internal::PlatformVersion(fingerprint);
  metadata.architecture = fingerprint.platform == "MacIntel" ? "x86" : "";
  metadata.model = "";
  metadata.mobile = false;
  metadata.bitness = "64";
  metadata.wow64 = false;
  return metadata;
}

}  // namespace clawbrowser

#endif  // CLAWBROWSER_USER_AGENT_METADATA_H_
