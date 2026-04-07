#ifndef CLAWBROWSER_CLI_ARGS_H_
#define CLAWBROWSER_CLI_ARGS_H_

#include <optional>
#include <string>

#include "base/command_line.h"

namespace clawbrowser {

// Parsed clawbrowser-specific CLI flags.
// Unknown flags pass through to Chromium unchanged.
class ClawArgs {
 public:
  ClawArgs();
  ClawArgs(const ClawArgs&);
  ClawArgs& operator=(const ClawArgs&);
  ClawArgs(ClawArgs&&);
  ClawArgs& operator=(ClawArgs&&);
  ~ClawArgs();
  static ClawArgs Parse(const base::CommandLine& command_line);

  bool has_fingerprint() const { return has_fingerprint_; }
  const std::string& fingerprint_id() const { return fingerprint_id_; }
  bool regenerate() const { return regenerate_; }
  bool list() const { return list_; }
  bool verbose() const { return verbose_; }
  bool json_output() const { return json_output_; }
  bool skip_verify() const { return skip_verify_; }
  bool verify_automation() const { return verify_automation_; }
  const std::string& country() const { return country_; }
  const std::optional<std::string>& city() const { return city_; }
  const std::optional<std::string>& connection_type() const {
    return connection_type_;
  }
  bool is_vanilla() const { return !has_fingerprint_ && !list_; }

 private:
  bool has_fingerprint_;
  std::string fingerprint_id_;
  bool regenerate_;
  bool list_;
  bool verbose_;
  bool json_output_;
  bool skip_verify_;
  bool verify_automation_;
  std::string country_;
  std::optional<std::string> city_;
  std::optional<std::string> connection_type_;
};

}  // namespace clawbrowser

#endif  // CLAWBROWSER_CLI_ARGS_H_
