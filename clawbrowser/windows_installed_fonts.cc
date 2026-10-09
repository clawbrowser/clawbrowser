#include "clawbrowser/windows_installed_fonts.h"

#include <dwrite.h>
#include <wrl/client.h>

#include "base/strings/utf_string_conversions.h"

namespace clawbrowser {

namespace {

std::optional<std::string> FamilyName(IDWriteFontFamily* family) {
  Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names;
  if (FAILED(family->GetFamilyNames(&names))) {
    return std::nullopt;
  }
  UINT32 index = 0;
  BOOL exists = FALSE;
  if (FAILED(names->FindLocaleName(L"en-us", &index, &exists)) || !exists) {
    index = 0;
  }
  UINT32 length = 0;
  if (FAILED(names->GetStringLength(index, &length))) {
    return std::nullopt;
  }
  std::wstring name(length + 1, L'\0');
  if (FAILED(names->GetString(index, name.data(), length + 1))) {
    return std::nullopt;
  }
  name.resize(length);
  return base::WideToUTF8(name);
}

}  // namespace

std::optional<std::vector<std::string>> GetInstalledFontFamilies() {
  Microsoft::WRL::ComPtr<IDWriteFactory> factory;
  if (FAILED(DWriteCreateFactory(
          DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
          reinterpret_cast<IUnknown**>(factory.GetAddressOf())))) {
    return std::nullopt;
  }
  Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
  if (FAILED(factory->GetSystemFontCollection(&collection, FALSE))) {
    return std::nullopt;
  }
  std::vector<std::string> families;
  const UINT32 count = collection->GetFontFamilyCount();
  families.reserve(count);
  for (UINT32 i = 0; i < count; ++i) {
    Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
    if (FAILED(collection->GetFontFamily(i, &family))) {
      continue;
    }
    if (auto name = FamilyName(family.Get())) {
      families.push_back(std::move(*name));
    }
  }
  return families;
}

}  // namespace clawbrowser
