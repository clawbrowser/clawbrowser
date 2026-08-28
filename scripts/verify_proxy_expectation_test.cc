#include <cassert>
#include <optional>
#include <string>

#include "clawbrowser/verify/proxy_expectation.h"

int main() {
  {
    clawbrowser::ProxyExpectation expected =
        clawbrowser::ResolveProxyExpectation("US", std::nullopt,
                                             std::string("US"));
    assert(expected.country.has_value());
    assert(*expected.country == "US");
    assert(!expected.city.has_value());
  }

  {
    clawbrowser::ProxyExpectation expected =
        clawbrowser::ResolveProxyExpectation(
            "US", std::make_optional<std::string>("Las Vegas"),
            std::string("US"));
    assert(expected.country.has_value());
    assert(*expected.country == "US");
    assert(expected.city.has_value());
    assert(*expected.city == "Las Vegas");
  }

  {
    clawbrowser::ProxyExpectation expected =
        clawbrowser::ResolveProxyExpectation(
            std::string(), std::make_optional<std::string>("Las Vegas"),
            std::string("US"));
    assert(expected.country.has_value());
    assert(*expected.country == "US");
    assert(expected.city.has_value());
    assert(*expected.city == "Las Vegas");
  }

  return 0;
}
