#include "clawbrowser/cli/api_client.h"

#include <optional>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace clawbrowser {

namespace {

constexpr int kTimeoutSeconds = 10;
constexpr int kMaxResponseBytes = 256 * 1024;  // 256KB

net::NetworkTrafficAnnotationTag GetTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("clawbrowser_api", R"(
    semantics {
      sender: "Clawbrowser"
      description: "Fetches fingerprint profiles and verifies proxy configuration."
      trigger: "User launches browser with --fingerprint flag."
      data: "API key, requested fingerprint parameters."
      destination: OTHER
    }
    policy {
      cookies_allowed: NO
      setting: "Controlled by --fingerprint CLI flag."
    })");
}

ApiError ParseApiError(int http_status, const std::string& body) {
  ApiError error;
  error.http_status = http_status;

  auto parsed = base::JSONReader::Read(body, base::JSON_PARSE_RFC);
  if (parsed && parsed->is_dict()) {
    const auto& dict = parsed->GetDict();
    if (const std::string* code = dict.FindString("code"))
      error.code = *code;
    if (const std::string* msg = dict.FindString("message"))
      error.message = *msg;
  }

  if (error.message.empty()) {
    switch (http_status) {
      case 401: error.message = "invalid API key"; break;
      case 429: error.message = "rate limited, try again later"; break;
      case 500: error.message = "API server error"; break;
      default: error.message = "unexpected error"; break;
    }
  }

  return error;
}

}  // namespace

ApiClient::ApiClient(const std::string& base_url,
                     const std::string& api_key,
                     scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : base_url_(base_url),
      api_key_(api_key),
      url_loader_factory_(std::move(url_loader_factory)) {}

ApiClient::~ApiClient() = default;

void ApiClient::GenerateFingerprint(const GenerateRequest& params,
                                    GenerateCallback callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(base_url_ + "/v1/fingerprints/generate");
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Authorization",
                                      "Bearer " + api_key_);
  resource_request->headers.SetHeader("Content-Type", "application/json");

  std::string body_json = params.ToJson();

  loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), GetTrafficAnnotation());
  loader_->AttachStringForUpload(body_json, "application/json");
  loader_->SetAllowHttpErrorResults(true);
  loader_->SetTimeoutDuration(base::Seconds(kTimeoutSeconds));

  loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          [](GenerateCallback callback,
             network::SimpleURLLoader* loader,
             std::optional<std::string> body) {
            int status = 0;
            if (loader->ResponseInfo() && loader->ResponseInfo()->headers)
              status = loader->ResponseInfo()->headers->response_code();

            if (!body || status == 0) {
              std::move(callback).Run(base::unexpected(ApiError{
                  0, "network_error",
                  "cannot reach API: " + net::ErrorToString(loader->NetError())
              }));
              return;
            }

            if (status != 200) {
              std::move(callback).Run(
                  base::unexpected(ParseApiError(status, *body)));
              return;
            }

            auto parsed = GenerateResponse::FromJson(*body);
            if (!parsed.has_value()) {
              std::move(callback).Run(base::unexpected(ApiError{
                  200, "parse_error",
                  "failed to parse API response: " + parsed.error()
              }));
              return;
            }

            std::move(callback).Run(base::ok(std::move(*parsed)));
          },
          std::move(callback), loader_.get()),
      kMaxResponseBytes);
}

void ApiClient::VerifyProxy(const VerifyProxyRequest& request,
                            VerifyCallback callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(base_url_ + "/v1/proxy/verify");
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Authorization",
                                      "Bearer " + api_key_);
  resource_request->headers.SetHeader("Content-Type", "application/json");

  std::string body_json = request.ToJson();

  loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), GetTrafficAnnotation());
  loader_->AttachStringForUpload(body_json, "application/json");
  loader_->SetAllowHttpErrorResults(true);
  loader_->SetTimeoutDuration(base::Seconds(kTimeoutSeconds));

  loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          [](VerifyCallback callback,
             network::SimpleURLLoader* loader,
             std::optional<std::string> body) {
            int status = 0;
            if (loader->ResponseInfo() && loader->ResponseInfo()->headers)
              status = loader->ResponseInfo()->headers->response_code();

            if (!body || status == 0) {
              std::move(callback).Run(base::unexpected(ApiError{
                  0, "network_error",
                  "cannot reach API: " + net::ErrorToString(loader->NetError())
              }));
              return;
            }

            if (status != 200) {
              std::move(callback).Run(
                  base::unexpected(ParseApiError(status, *body)));
              return;
            }

            auto parsed = VerifyProxyResponse::FromJson(*body);
            if (!parsed.has_value()) {
              std::move(callback).Run(base::unexpected(ApiError{
                  200, "parse_error",
                  "failed to parse verify response: " + parsed.error()
              }));
              return;
            }

            std::move(callback).Run(base::ok(std::move(*parsed)));
          },
          std::move(callback), loader_.get()),
      kMaxResponseBytes);
}

}  // namespace clawbrowser
