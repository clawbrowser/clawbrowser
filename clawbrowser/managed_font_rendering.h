#ifndef CLAWBROWSER_MANAGED_FONT_RENDERING_H_
#define CLAWBROWSER_MANAGED_FONT_RENDERING_H_

#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkFontTypes.h"

namespace clawbrowser {

// Call only for a managed, closed-catalog profile. Match the Fontations strike
// settings of the Linux catalog path without inheriting host font smoothing.
// Font identity, size, synthetic style and transforms remain the caller's own.
inline void ApplyManagedFontRendering(SkFont& font, bool geometric_precision) {
  font.setSubpixel(true);
  font.setLinearMetrics(true);
  font.setEmbeddedBitmaps(false);
  font.setForceAutoHinting(false);
  font.setEdging(SkFont::Edging::kSubpixelAntiAlias);
  font.setHinting(geometric_precision ? SkFontHinting::kNone
                                    : SkFontHinting::kNormal);
}

}  // namespace clawbrowser

#endif  // CLAWBROWSER_MANAGED_FONT_RENDERING_H_
