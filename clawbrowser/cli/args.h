#ifndef CLAWBROWSER_CLI_ARGS_H_
#define CLAWBROWSER_CLI_ARGS_H_

#include <optional>
#include <string>

#include "base/command_line.h"

namespace clawbrowser {

inline constexpr char kEnableCanvasSpoofingSwitch[] =
    "enable-canvas-spoofing";
inline constexpr char kDisableCanvasSpoofingSwitch[] =
    "disable-canvas-spoofing";
inline constexpr char kEnableWebGLSpoofingSwitch[] =
    "enable-webgl-spoofing";
inline constexpr char kDisableWebGLSpoofingSwitch[] =
    "disable-webgl-spoofing";
inline constexpr char kRequireProxySwitch[] = "clawbrowser-require-proxy";
// QA-only opt-in; not a persisted/backend policy or a production default.
inline constexpr char kExperimentalNormalizedCanvasSwitch[] =
    "clawbrowser-experimental-normalized-canvas";

// Parsed clawbrowser-specific CLI flags.
// Unknown flags pass through to the browser engine unchanged.
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
  bool require_proxy() const { return require_proxy_; }
  // Whether --enable-*-spoofing was passed. This is a *forcing* switch; the
  // fingerprint's surface_policy supplies the default when it is absent.
  bool canvas_spoofing_enabled() const { return canvas_spoofing_enabled_; }
  bool webgl_spoofing_enabled() const { return webgl_spoofing_enabled_; }

  // Whether --disable-*-spoofing was passed. Kept separate from the enable
  // switches so startup can tell "not requested" from "explicitly refused" --
  // only the latter should override what the fingerprint asks for.
  bool canvas_spoofing_suppressed() const { return canvas_spoofing_suppressed_; }
  bool webgl_spoofing_suppressed() const { return webgl_spoofing_suppressed_; }
  bool has_country_override() const { return has_country_override_; }
  bool has_city_override() const { return has_city_override_; }
  bool has_connection_type_override() const {
    return has_connection_type_override_;
  }
  bool has_proxy_scheme_override() const {
    return has_proxy_scheme_override_;
  }
  bool has_location_overrides() const {
    return has_country_override_ || has_city_override_ ||
           has_connection_type_override_;
  }
  const std::string& country() const { return country_; }
  const std::optional<std::string>& city() const { return city_; }
  const std::optional<std::string>& connection_type() const {
    return connection_type_;
  }
  const std::optional<std::string>& proxy_scheme() const {
    return proxy_scheme_;
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
  bool require_proxy_;
  bool canvas_spoofing_enabled_;
  bool webgl_spoofing_enabled_;
  bool canvas_spoofing_suppressed_;
  bool webgl_spoofing_suppressed_;
  bool has_country_override_;
  bool has_city_override_;
  bool has_connection_type_override_;
  bool has_proxy_scheme_override_;
  std::string country_;
  std::optional<std::string> city_;
  std::optional<std::string> connection_type_;
  std::optional<std::string> proxy_scheme_;
};

}  // namespace clawbrowser

#endif  // CLAWBROWSER_CLI_ARGS_H_
