#ifndef CLAWBROWSER_PROFILE_ENVELOPE_H_
#define CLAWBROWSER_PROFILE_ENVELOPE_H_

#include <optional>
#include <string>

#include "base/types/expected.h"
#include "clawbrowser/generated/fingerprint_types.h"

namespace clawbrowser {

// On-disk profile envelope wrapping the API GenerateResponse.
// Hand-written — the nested GenerateResponse is code-generated.
struct ProfileEnvelope {
  ProfileEnvelope();
  ProfileEnvelope(const ProfileEnvelope&);
  ProfileEnvelope& operator=(const ProfileEnvelope&);
  ProfileEnvelope(ProfileEnvelope&&);
  ProfileEnvelope& operator=(ProfileEnvelope&&);
  ~ProfileEnvelope();

  int schema_version = 0;
  std::string created_at;
  GenerateRequest request;
  GenerateResponse response;
  bool schema_outdated = false;  // Set if schema_version < current

  static constexpr int kCurrentSchemaVersion = 1;

  // Parse from JSON string.
  // Returns envelope on success, error message on failure.
  static base::expected<ProfileEnvelope, std::string> Parse(
      const std::string& json);

  // Serialize to JSON string for writing to disk.
  std::string Serialize() const;
};

}  // namespace clawbrowser

#endif  // CLAWBROWSER_PROFILE_ENVELOPE_H_
