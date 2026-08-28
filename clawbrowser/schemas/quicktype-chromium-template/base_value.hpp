// quicktype-chromium-template/base_value.hpp
//
// Historical note:
// The original implementation plan explored quicktype with a Chromium-specific
// base::Value template. That path was not adopted because quicktype's C++
// backend is tightly coupled to nlohmann/json.
//
// The authoritative generation pipeline is now:
//   1. extract_schemas.py -> clawbrowser/schemas/browser_schema.json
//   2. generate_browser_types.py -> clawbrowser/generated/fingerprint_types.{h,cc}
//
// This file remains only as working notes for the abandoned quicktype path so
// the repository history still explains why the custom template directory
// exists.
