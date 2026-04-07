#include "clawbrowser/profile_envelope.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"

namespace clawbrowser {

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

  return base::ok(std::move(envelope));
}

std::string ProfileEnvelope::Serialize() const {
  base::DictValue root;
  root.Set("schema_version", schema_version);
  root.Set("created_at", created_at);
  root.Set("request", request.ToDict());
  root.Set("response", response.ToDict());

  std::string output;
  base::JSONWriter::WriteWithOptions(
      base::Value(std::move(root)),
      base::JSONWriter::OPTIONS_PRETTY_PRINT, &output);
  return output;
}

}  // namespace clawbrowser
