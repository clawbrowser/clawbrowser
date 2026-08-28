#ifndef CLAWBROWSER_VERIFY_PROXY_EXPECTATION_H_
#define CLAWBROWSER_VERIFY_PROXY_EXPECTATION_H_

#include <optional>
#include <string>

namespace clawbrowser {

struct ProxyExpectation {
  ProxyExpectation();
  ProxyExpectation(const ProxyExpectation&);
  ProxyExpectation& operator=(const ProxyExpectation&);
  ProxyExpectation(ProxyExpectation&&);
  ProxyExpectation& operator=(ProxyExpectation&&);
  ~ProxyExpectation();

  std::optional<std::string> country;
  std::optional<std::string> city;
};

inline ProxyExpectation ResolveProxyExpectation(
    const std::string& requested_country,
    const std::optional<std::string>& requested_city,
    const std::optional<std::string>& generated_country) {
  ProxyExpectation expected;
  if (!requested_country.empty()) {
    expected.country = requested_country;
  } else if (generated_country.has_value() && !generated_country->empty()) {
    expected.country = *generated_country;
  }

  if (requested_city.has_value() && !requested_city->empty()) {
    expected.city = *requested_city;
  }

  return expected;
}

}  // namespace clawbrowser

#endif  // CLAWBROWSER_VERIFY_PROXY_EXPECTATION_H_
