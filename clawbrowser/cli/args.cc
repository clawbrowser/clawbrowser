#include "clawbrowser/cli/args.h"

namespace clawbrowser {

ClawArgs::ClawArgs()
    : has_fingerprint_(false),
      regenerate_(false),
      list_(false),
      verbose_(false),
      json_output_(false),
      skip_verify_(false),
      verify_automation_(false),
      canvas_spoofing_enabled_(false),
      webgl_spoofing_enabled_(false),
      has_country_override_(false),
      has_city_override_(false),
      has_connection_type_override_(false),
      has_proxy_scheme_override_(false) {}

ClawArgs::ClawArgs(const ClawArgs&) = default;

ClawArgs& ClawArgs::operator=(const ClawArgs&) = default;

ClawArgs::ClawArgs(ClawArgs&&) = default;

ClawArgs& ClawArgs::operator=(ClawArgs&&) = default;

ClawArgs::~ClawArgs() = default;

// static
ClawArgs ClawArgs::Parse(const base::CommandLine& command_line) {
  ClawArgs args;

  if (command_line.HasSwitch("fingerprint")) {
    args.has_fingerprint_ = true;
    args.fingerprint_id_ = command_line.GetSwitchValueASCII("fingerprint");
  }

  args.regenerate_ = command_line.HasSwitch("regenerate");
  args.list_ = command_line.HasSwitch("list");
  args.verbose_ = command_line.HasSwitch("verbose");
  args.skip_verify_ = command_line.HasSwitch("skip-verify");
  args.verify_automation_ = command_line.HasSwitch("verify-automation");
  args.canvas_spoofing_enabled_ =
      command_line.HasSwitch(kEnableCanvasSpoofingSwitch) &&
      !command_line.HasSwitch(kDisableCanvasSpoofingSwitch);
  args.webgl_spoofing_enabled_ =
      command_line.HasSwitch(kEnableWebGLSpoofingSwitch) &&
      !command_line.HasSwitch(kDisableWebGLSpoofingSwitch);
  args.has_country_override_ = command_line.HasSwitch("country");
  args.has_city_override_ = command_line.HasSwitch("city");
  args.has_connection_type_override_ =
      command_line.HasSwitch("connection-type");
  args.has_proxy_scheme_override_ = command_line.HasSwitch("proxy-scheme");
  args.country_ = command_line.GetSwitchValueASCII("country");

  std::string city = command_line.GetSwitchValueASCII("city");
  if (!city.empty()) {
    args.city_ = city;
  }

  std::string connection_type =
      command_line.GetSwitchValueASCII("connection-type");
  if (!connection_type.empty()) {
    args.connection_type_ = connection_type;
  }

  std::string proxy_scheme = command_line.GetSwitchValueASCII("proxy-scheme");
  if (!proxy_scheme.empty()) {
    args.proxy_scheme_ = proxy_scheme;
  }

  if (command_line.HasSwitch("output")) {
    args.json_output_ =
        command_line.GetSwitchValueASCII("output") == "json";
  }

  return args;
}

}  // namespace clawbrowser
