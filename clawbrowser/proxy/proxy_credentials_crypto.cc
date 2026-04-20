#include "clawbrowser/proxy/proxy_credentials_crypto.h"

#include <cstdint>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "crypto/aead.h"

namespace clawbrowser {
namespace {

constexpr char kEncryptedBlobVersion[] = "v1";
constexpr uint8_t kProxyCredentialsAesKey[] = {
    0xad, 0xcd, 0x15, 0x2e, 0xa2, 0x74, 0x04, 0xbe,
    0x85, 0xdf, 0x8a, 0x26, 0xa3, 0x57, 0xf9, 0x12,
    0xee, 0x5a, 0xe7, 0x1e, 0x95, 0xa5, 0x7c, 0xca,
    0xbc, 0x85, 0xa5, 0x02, 0xbc, 0x88, 0x96, 0x58,
};

static_assert(sizeof(kProxyCredentialsAesKey) == 32,
              "proxy credential AES key must be 32 bytes");

base::expected<std::string, std::string> SerializeCredentialPayload(
    const std::string& username,
    const std::string& password) {
  base::DictValue payload;
  payload.Set("username", username);
  payload.Set("password", password);

  std::string json;
  if (!base::JSONWriter::Write(base::Value(std::move(payload)), &json)) {
    return base::unexpected(
        "failed to serialize proxy credential payload");
  }
  return base::ok(std::move(json));
}

base::expected<ProxyCredentialsSecret, std::string> ParseCredentialPayload(
    const std::string& payload) {
  std::optional<base::Value> parsed =
      base::JSONReader::Read(payload, base::JSON_PARSE_RFC);
  if (!parsed || !parsed->is_dict()) {
    return base::unexpected(
        "failed to parse decrypted proxy credential payload");
  }

  const base::DictValue& dict = parsed->GetDict();
  const std::string* username = dict.FindString("username");
  const std::string* password = dict.FindString("password");
  if (!username || !password) {
    return base::unexpected(
        "decrypted proxy credential payload missing fields");
  }

  ProxyCredentialsSecret credentials;
  credentials.username = *username;
  credentials.password = *password;
  return base::ok(std::move(credentials));
}

base::expected<void, std::string> SplitEncryptedBlob(
    const std::string& encrypted_blob,
    std::string* nonce_base64,
    std::string* ciphertext_base64) {
  const size_t first_separator = encrypted_blob.find(':');
  const size_t second_separator =
      first_separator == std::string::npos
          ? std::string::npos
          : encrypted_blob.find(':', first_separator + 1);
  if (first_separator == std::string::npos ||
      second_separator == std::string::npos) {
    return base::unexpected("encrypted proxy credential blob is malformed");
  }

  if (encrypted_blob.substr(0, first_separator) != kEncryptedBlobVersion) {
    return base::unexpected("unsupported encrypted proxy credential version");
  }

  *nonce_base64 = encrypted_blob.substr(first_separator + 1,
                                        second_separator - first_separator - 1);
  *ciphertext_base64 = encrypted_blob.substr(second_separator + 1);
  if (nonce_base64->empty() || ciphertext_base64->empty()) {
    return base::unexpected("encrypted proxy credential blob is incomplete");
  }

  return base::ok();
}

}  // namespace

base::expected<std::string, std::string> EncryptProxyCredentials(
    const std::string& username,
    const std::string& password) {
  auto payload = SerializeCredentialPayload(username, password);
  if (!payload.has_value()) {
    return base::unexpected(payload.error());
  }

  const std::string key(
      reinterpret_cast<const char*>(kProxyCredentialsAesKey),
      sizeof(kProxyCredentialsAesKey));
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(&key);

  const std::string nonce = base::RandBytesAsString(aead.NonceLength());
  std::string ciphertext;
  if (!aead.Seal(*payload, nonce, std::string(), &ciphertext)) {
    return base::unexpected("failed to encrypt proxy credentials");
  }

  return base::ok(std::string(kEncryptedBlobVersion) + ":" +
                  base::Base64Encode(nonce) + ":" +
                  base::Base64Encode(ciphertext));
}

base::expected<ProxyCredentialsSecret, std::string> DecryptProxyCredentials(
    const std::string& encrypted_blob) {
  std::string nonce_base64;
  std::string ciphertext_base64;
  auto split_result = SplitEncryptedBlob(encrypted_blob, &nonce_base64,
                                         &ciphertext_base64);
  if (!split_result.has_value()) {
    return base::unexpected(split_result.error());
  }

  std::string nonce;
  if (!base::Base64Decode(nonce_base64, &nonce)) {
    return base::unexpected(
        "failed to decode encrypted proxy credential nonce");
  }

  std::string ciphertext;
  if (!base::Base64Decode(ciphertext_base64, &ciphertext)) {
    return base::unexpected(
        "failed to decode encrypted proxy credential ciphertext");
  }

  const std::string key(
      reinterpret_cast<const char*>(kProxyCredentialsAesKey),
      sizeof(kProxyCredentialsAesKey));
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(&key);

  std::string plaintext;
  if (!aead.Open(ciphertext, nonce, std::string(), &plaintext)) {
    return base::unexpected("failed to decrypt proxy credentials");
  }

  return ParseCredentialPayload(plaintext);
}

}  // namespace clawbrowser
