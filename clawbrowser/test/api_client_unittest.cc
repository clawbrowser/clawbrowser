#include "clawbrowser/cli/api_client.h"

#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_version.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

const char kTestApiKey[] = "test_api_key_123";
const char kTestBaseUrl[] = "https://api.clawbrowser.ai";

class ApiClientTest : public testing::Test {
 protected:
  static std::string HttpStatusLine(net::HttpStatusCode status) {
    switch (status) {
      case net::HTTP_UNAUTHORIZED:
        return "401 Unauthorized";
      case net::HTTP_TOO_MANY_REQUESTS:
        return "429 Too Many Requests";
      default:
        return base::StringPrintf("%d", static_cast<int>(status));
    }
  }

  GenerateRequest MakeGenerateRequest() const {
    GenerateRequest request;
    request.platform = "macos";
    request.browser = "chrome";
    request.country = "US";
    return request;
  }

  VerifyProxyRequest MakeVerifyProxyRequest() const {
    VerifyProxyRequest request;
    request.proxy.host = "proxy.example.com";
    request.proxy.port = 8080;
    request.proxy.username = "user";
    request.proxy.password = "pass";
    request.expected_country = "US";
    request.expected_city = std::string("New York");
    return request;
  }

  void SetUp() override {
    client_ = std::make_unique<ApiClient>(
        kTestBaseUrl, kTestApiKey,
        url_loader_factory_.GetSafeWeakWrapper());
  }

  void AddHttpErrorResponse(const std::string& path,
                            const std::string& body,
                            net::HttpStatusCode status) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1),
                                          HttpStatusLine(status))
            .Build();
    url_loader_factory_.AddResponse(
        GURL(std::string(kTestBaseUrl) + path), std::move(head), body,
        network::URLLoaderCompletionStatus(net::OK));
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<ApiClient> client_;
};

TEST_F(ApiClientTest, GenerateFingerprintSuccess) {
  std::string response_json = R"({
    "fingerprint": {
      "browser_family": "chrome",
      "browser_version": "120.0.0.0",
      "engine": "blink",
      "os": "macos",
      "os_version": "10.15.7",
      "architecture": "arm64",
      "device_class": "desktop",
      "user_agent_data": {"brands": [{"brand": "Chromium", "version": "120"}], "fullVersionList": [{"brand": "Chromium", "version": "120.0.0.0"}], "platform": "macOS", "platformVersion": "10.15.7", "architecture": "arm", "bitness": "64", "mobile": false, "model": ""},
      "headers": {"Accept-Language": "en-US"},
      "surface_policy": {"canvas": {"mode": "native"}, "audio": {"mode": "native"}, "client_rects": {"mode": "native"}, "webgl": {"mode": "native"}, "fonts": {"mode": "native_or_allowlist"}, "plugins": {"mode": "override"}, "media_devices": {"mode": "override"}, "speech_voices": {"mode": "override"}},
      "user_agent": "test-ua", "platform": "test",
      "screen": {"width": 1920, "height": 1080, "avail_width": 1920,
                 "avail_height": 1040, "color_depth": 24, "pixel_ratio": 1.0},
      "hardware": {"concurrency": 8, "memory": 8},
      "webgl": {"vendor": "test", "renderer": "test"},
      "canvas_seed": 123, "audio_seed": 456, "client_rects_seed": 789,
      "timezone": "UTC", "language": ["en"], "fonts": ["Arial"]
    }
  })";

  url_loader_factory_.AddResponse(
      kTestBaseUrl + std::string("/v1/fingerprints/generate"),
      response_json);

  base::test::TestFuture<base::expected<GenerateResponse, ApiError>> future;
  client_->GenerateFingerprint(MakeGenerateRequest(), future.GetCallback());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value()) << result.error().message;
  EXPECT_EQ(result->fingerprint.user_agent, "test-ua");
}

TEST_F(ApiClientTest, GenerateFingerprintUnauthorized) {
  AddHttpErrorResponse(
      "/v1/fingerprints/generate",
      R"({"code": "invalid_api_key", "message": "invalid API key"})",
      net::HTTP_UNAUTHORIZED);

  base::test::TestFuture<base::expected<GenerateResponse, ApiError>> future;
  client_->GenerateFingerprint(MakeGenerateRequest(), future.GetCallback());

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().http_status, 401);
}

TEST_F(ApiClientTest, GenerateFingerprintRateLimited) {
  AddHttpErrorResponse(
      "/v1/fingerprints/generate",
      R"({"code": "rate_limited", "message": "rate limited"})",
      net::HTTP_TOO_MANY_REQUESTS);

  base::test::TestFuture<base::expected<GenerateResponse, ApiError>> future;
  client_->GenerateFingerprint(MakeGenerateRequest(), future.GetCallback());

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().http_status, 429);
}

TEST_F(ApiClientTest, VerifyProxySuccess) {
  url_loader_factory_.AddResponse(
      kTestBaseUrl + std::string("/v1/proxy/verify"),
      R"({"match": true, "actual_country": "US", "actual_city": "New York"})");

  base::test::TestFuture<base::expected<VerifyProxyResponse, ApiError>> future;
  client_->VerifyProxy(MakeVerifyProxyRequest(), future.GetCallback());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->match);
  EXPECT_EQ(result->actual_country, "US");
}

TEST_F(ApiClientTest, VerifyProxyRequestRequiresProxyCredentials) {
  auto parsed = VerifyProxyRequest::FromJson(R"({
    "proxy": {
      "host": "proxy.example.com",
      "port": 8080,
      "username": "user",
      "password": "pass"
    },
    "expected_country": "US"
  })");

  ASSERT_TRUE(parsed.has_value()) << parsed.error();
  EXPECT_EQ(parsed->proxy.host, "proxy.example.com");
  EXPECT_EQ(parsed->proxy.port, 8080);
  EXPECT_EQ(parsed->proxy.username, "user");
  EXPECT_EQ(parsed->proxy.password, "pass");
  EXPECT_EQ(parsed->expected_country, "US");
}

TEST_F(ApiClientTest, NetworkFailure) {
  url_loader_factory_.AddResponse(
      GURL(kTestBaseUrl + std::string("/v1/fingerprints/generate")),
      network::mojom::URLResponseHead::New(),
      "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_REFUSED));

  base::test::TestFuture<base::expected<GenerateResponse, ApiError>> future;
  client_->GenerateFingerprint(MakeGenerateRequest(), future.GetCallback());

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().http_status, 0);
  EXPECT_NE(result.error().message.find("reach"), std::string::npos);
}

}  // namespace
}  // namespace clawbrowser
