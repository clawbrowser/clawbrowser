#include "clawbrowser/profile_envelope.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "clawbrowser/proxy/proxy_credentials_crypto.h"

namespace clawbrowser {
namespace {

constexpr char kEncryptedProxyCredentialsKey[] = "encrypted_proxy_credentials";

bool HasPersistableProxyCredentials(const ProxyConfig& proxy) {
  return proxy.username.has_value() && !proxy.username->empty() &&
         proxy.password.has_value() && !proxy.password->empty();
}

base::DictValue SerializePersistedProxyConfig(const ProxyConfig& proxy) {
  base::DictValue dict;
  if (proxy.scheme.has_value())
    dict.Set("scheme", *proxy.scheme);
  if (proxy.country.has_value())
    dict.Set("country", *proxy.country);
  if (proxy.city.has_value())
    dict.Set("city", *proxy.city);
  if (proxy.connection_type.has_value())
    dict.Set("connection_type", *proxy.connection_type);
  if (proxy.host.has_value())
    dict.Set("host", *proxy.host);
  if (proxy.port.has_value())
    dict.Set("port", *proxy.port);
  return dict;
}

base::expected<void, std::string> RestoreEncryptedProxyCredentials(
    const base::DictValue& root,
    GenerateResponse* response) {
  const std::string* encrypted =
      root.FindString(kEncryptedProxyCredentialsKey);
  if (!encrypted) {
    return base::ok();
  }

  if (!response->proxy.has_value()) {
    return base::unexpected(
        "encrypted proxy credentials present without proxy config");
  }

  auto decrypted = DecryptProxyCredentials(*encrypted);
  if (!decrypted.has_value()) {
    return base::unexpected("failed to restore encrypted proxy credentials: " +
                            decrypted.error());
  }

  response->proxy->username = decrypted->username;
  response->proxy->password = decrypted->password;
  return base::ok();
}

}  // namespace

ProfileEnvelope::ProfileEnvelope() = default;
ProfileEnvelope::ProfileEnvelope(const ProfileEnvelope&) = default;
ProfileEnvelope& ProfileEnvelope::operator=(const ProfileEnvelope&) = default;
ProfileEnvelope::ProfileEnvelope(ProfileEnvelope&&) = default;
ProfileEnvelope& ProfileEnvelope::operator=(ProfileEnvelope&&) = default;
ProfileEnvelope::~ProfileEnvelope() = default;

// static
base::expected<ProfileEnvelope, std::string> ProfileEnvelope::Parse(
    const std::string& json) {
  auto parsed = base::JSONReader::Read(json, base::JSON_PARSE_RFC);
  if (!parsed || !parsed->is_dict()) {
    return base::unexpected("failed to parse JSON");
  }

  const base::DictValue& root = parsed->GetDict();
  ProfileEnvelope envelope;

  // schema_version
  auto schema_version = root.FindInt("schema_version");
  envelope.schema_version = schema_version ? *schema_version : 0;
  envelope.schema_outdated =
      !schema_version || envelope.schema_version < kCurrentSchemaVersion;

  const std::string* profile_id = root.FindString("profile_id");
  if (profile_id) {
    envelope.profile_id = *profile_id;
  }

  // created_at
  const std::string* created_at = root.FindString("created_at");
  if (!created_at) {
    return base::unexpected("fingerprint missing field: created_at");
  }
  envelope.created_at = *created_at;

  // request
  const base::DictValue* request = root.FindDict("request");
  if (request) {
    auto parsed_request = GenerateRequest::FromDict(*request);
    if (!parsed_request.has_value()) {
      return base::unexpected("failed to parse request: " +
                              parsed_request.error());
    }
    envelope.request = std::move(*parsed_request);
  }

  // response — delegate to generated parser
  const base::DictValue* response = root.FindDict("response");
  if (!response) {
    return base::unexpected("fingerprint missing field: response");
  }

  auto generate_response = GenerateResponse::FromDict(*response);
  if (!generate_response.has_value()) {
    return base::unexpected("failed to parse response: " +
                            generate_response.error());
  }
  envelope.response = std::move(*generate_response);

  auto restore_result =
      RestoreEncryptedProxyCredentials(root, &envelope.response);
  if (!restore_result.has_value()) {
    return base::unexpected(restore_result.error());
  }

  return base::ok(std::move(envelope));
}

std::string ProfileEnvelope::Serialize() const {
  base::DictValue root;
  root.Set("schema_version", std::max(schema_version, kCurrentSchemaVersion));
  if (profile_id.has_value()) {
    root.Set("profile_id", *profile_id);
  }
  root.Set("created_at", created_at);
  root.Set("request", request.ToDict());

  base::DictValue response_dict = response.ToDict();
  if (response.proxy.has_value()) {
    response_dict.Set("proxy", SerializePersistedProxyConfig(*response.proxy));
    if (HasPersistableProxyCredentials(*response.proxy)) {
      auto encrypted = EncryptProxyCredentials(*response.proxy->username,
                                               *response.proxy->password);
      CHECK(encrypted.has_value());
      root.Set(kEncryptedProxyCredentialsKey, *encrypted);
    }
  }
  root.Set("response", std::move(response_dict));

  std::string output;
  base::JSONWriter::WriteWithOptions(
      base::Value(std::move(root)),
      base::JSONWriter::OPTIONS_PRETTY_PRINT, &output);
  return output;
}

}  // namespace clawbrowser
