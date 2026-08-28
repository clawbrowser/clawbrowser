#include "clawbrowser/proxy/proxy_credentials_crypto.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

TEST(ProxyCredentialsCryptoTest, RoundTripsCredentials) {
  auto encrypted = EncryptProxyCredentials("user_abc", "pass_xyz");
  ASSERT_TRUE(encrypted.has_value()) << encrypted.error();

  auto decrypted = DecryptProxyCredentials(*encrypted);
  ASSERT_TRUE(decrypted.has_value()) << decrypted.error();
  EXPECT_EQ(decrypted->username, "user_abc");
  EXPECT_EQ(decrypted->password, "pass_xyz");
}

TEST(ProxyCredentialsCryptoTest, GeneratesDifferentCiphertextPerCall) {
  auto first = EncryptProxyCredentials("user_abc", "pass_xyz");
  auto second = EncryptProxyCredentials("user_abc", "pass_xyz");
  ASSERT_TRUE(first.has_value()) << first.error();
  ASSERT_TRUE(second.has_value()) << second.error();
  EXPECT_NE(*first, *second);
}

TEST(ProxyCredentialsCryptoTest, RejectsMalformedCiphertext) {
  auto decrypted = DecryptProxyCredentials("v1:not-valid-base64:still-bad");
  ASSERT_FALSE(decrypted.has_value());
  EXPECT_NE(decrypted.error().find("decode"), std::string::npos);
}

}  // namespace
}  // namespace clawbrowser
