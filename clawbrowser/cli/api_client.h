#ifndef CLAWBROWSER_CLI_API_CLIENT_H_
#define CLAWBROWSER_CLI_API_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "clawbrowser/generated/fingerprint_types.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace clawbrowser {

struct ApiError {
  int http_status = 0;  // 0 = network error
  std::string code;
  std::string message;
};

// HTTP client for clawbrowser API. Used pre-launch in browser process.
// Only two endpoints: generate fingerprint and verify proxy.
// 10s timeout, no retries — fail fast.
class ApiClient {
 public:
  ApiClient(const std::string& base_url,
            const std::string& api_key,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~ApiClient();

  using GenerateCallback =
      base::OnceCallback<void(base::expected<GenerateResponse, ApiError>)>;
  using VerifyCallback =
      base::OnceCallback<void(base::expected<VerifyProxyResponse, ApiError>)>;

  void GenerateFingerprint(const GenerateRequest& params,
                           GenerateCallback callback);

  void VerifyProxy(const VerifyProxyRequest& request,
                   VerifyCallback callback);

 private:
  std::string base_url_;
  std::string api_key_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
};

}  // namespace clawbrowser

#endif  // CLAWBROWSER_CLI_API_CLIENT_H_
