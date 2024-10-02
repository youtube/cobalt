// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_proxy_impl_win.h"

#include <shlobj.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/renderer_host/dwrite_font_file_util_win.h"
#include "content/browser/renderer_host/dwrite_font_uma_logging_win.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_unique_name_table.pb.h"
#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"
#include "ui/gfx/win/direct_write.h"
#include "ui/gfx/win/text_analysis_source.h"

namespace mswr = Microsoft::WRL;

namespace content {

namespace {

// These are the fonts that Blink tries to load in getLastResortFallbackFont,
// and will crash if none can be loaded.
const wchar_t* kLastResortFontNames[] = {
    L"Sans",     L"Arial",   L"MS UI Gothic",    L"Microsoft Sans Serif",
    L"Segoe UI", L"Calibri", L"Times New Roman", L"Courier New"};

struct RequiredFontStyle {
  const char16_t* family_name;
  DWRITE_FONT_WEIGHT required_weight;
  DWRITE_FONT_STRETCH required_stretch;
  DWRITE_FONT_STYLE required_style;
};

// Used in tests to allow a known font to masquerade as a locally installed
// font. Usually this is the Ahem.ttf font. Leaked at shutdown.
std::vector<base::FilePath>* g_sideloaded_fonts = nullptr;

const RequiredFontStyle kRequiredStyles[] = {
    // The regular version of Gill Sans is actually in the Gill Sans MT family,
    // and the Gill Sans family typically contains just the ultra-bold styles.
    {u"gill sans", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
     DWRITE_FONT_STYLE_NORMAL},
    {u"helvetica", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
     DWRITE_FONT_STYLE_NORMAL},
    {u"open sans", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
     DWRITE_FONT_STYLE_NORMAL},
};

// As a workaround for crbug.com/635932, refuse to load some common fonts that
// do not contain certain styles. We found that sometimes these fonts are
// installed only in specialized styles ('Open Sans' might only be available in
// the condensed light variant, or Helvetica might only be available in bold).
// That results in a poor user experience because websites that use those fonts
// usually expect them to be rendered in the regular variant.
bool CheckRequiredStylesPresent(IDWriteFontCollection* collection,
                                const std::u16string& family_name,
                                uint32_t family_index) {
  for (const auto& font_style : kRequiredStyles) {
    if (base::EqualsCaseInsensitiveASCII(family_name, font_style.family_name)) {
      mswr::ComPtr<IDWriteFontFamily> family;
      if (FAILED(collection->GetFontFamily(family_index, &family))) {
        DCHECK(false);
        return true;
      }
      mswr::ComPtr<IDWriteFont> font;
      if (FAILED(family->GetFirstMatchingFont(
              font_style.required_weight, font_style.required_stretch,
              font_style.required_style, &font))) {
        DCHECK(false);
        return true;
      }

      // GetFirstMatchingFont doesn't require strict style matching, so check
      // the actual font that we got.
      if (font->GetWeight() != font_style.required_weight ||
          font->GetStretch() != font_style.required_stretch ||
          font->GetStyle() != font_style.required_style) {
        // Not really a loader type, but good to have telemetry on how often
        // fonts like these are encountered, and the data can be compared with
        // the other loader types.
        LogLoaderType(
            DirectWriteFontLoaderType::FONT_WITH_MISSING_REQUIRED_STYLES);
        return false;
      }
      break;
    }
  }
  return true;
}

HRESULT GetLocalFontCollection(mswr::ComPtr<IDWriteFactory3>& factory,
                               IDWriteFontCollection** collection) {
  if (!g_sideloaded_fonts) {
    // Normal path - use the system's font collection with no sideloading.
    return factory->GetSystemFontCollection(collection);
  }
  // If sideloading - build a font set with sideloads then add the system font
  // collection.
  mswr::ComPtr<IDWriteFontSetBuilder> font_set_builder;
  HRESULT hr = factory->CreateFontSetBuilder(&font_set_builder);
  if (!SUCCEEDED(hr)) {
    return hr;
  }
  for (auto& path : *g_sideloaded_fonts) {
    mswr::ComPtr<IDWriteFontFile> font_file;
    hr = factory->CreateFontFileReference(path.value().c_str(), nullptr,
                                          &font_file);
    if (!SUCCEEDED(hr)) {
      return hr;
    }
    BOOL supported;
    DWRITE_FONT_FILE_TYPE file_type;
    UINT32 n_fonts;
    hr = font_file->Analyze(&supported, &file_type, nullptr, &n_fonts);
    if (!SUCCEEDED(hr)) {
      return hr;
    }
    for (UINT32 font_index = 0; font_index < n_fonts; ++font_index) {
      mswr::ComPtr<IDWriteFontFaceReference> font_face;
      hr = factory->CreateFontFaceReference(font_file.Get(), font_index,
                                            DWRITE_FONT_SIMULATIONS_NONE,
                                            &font_face);
      if (!SUCCEEDED(hr)) {
        return hr;
      }
      hr = font_set_builder->AddFontFaceReference(font_face.Get());
      if (!SUCCEEDED(hr)) {
        return hr;
      }
    }
  }
  // Now add the system fonts.
  mswr::ComPtr<IDWriteFontSet> system_font_set;
  hr = factory->GetSystemFontSet(&system_font_set);
  if (!SUCCEEDED(hr)) {
    return hr;
  }
  hr = font_set_builder->AddFontSet(system_font_set.Get());
  if (!SUCCEEDED(hr)) {
    return hr;
  }
  // Make the set.
  mswr::ComPtr<IDWriteFontSet> font_set;
  hr = font_set_builder->CreateFontSet(&font_set);
  if (!SUCCEEDED(hr)) {
    return hr;
  }
  // Make the collection.
  mswr::ComPtr<IDWriteFontCollection1> collection1;
  hr = factory->CreateFontCollectionFromFontSet(font_set.Get(), &collection1);
  if (!SUCCEEDED(hr)) {
    return hr;
  }
  hr = collection1->QueryInterface(collection);
  return hr;
}

}  // namespace

DWriteFontProxyImpl::DWriteFontProxyImpl()
    : windows_fonts_path_(GetWindowsFontsPath()) {}

DWriteFontProxyImpl::~DWriteFontProxyImpl() = default;

// static
void DWriteFontProxyImpl::Create(
    mojo::PendingReceiver<blink::mojom::DWriteFontProxy> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<DWriteFontProxyImpl>(),
                              std::move(receiver));
}

// static
void DWriteFontProxyImpl::SideLoadFontForTesting(base::FilePath path) {
  if (!g_sideloaded_fonts) {
    // Note: this list is leaked.
    g_sideloaded_fonts = new std::vector<base::FilePath>();
  }
  g_sideloaded_fonts->push_back(path);
}

void DWriteFontProxyImpl::SetWindowsFontsPathForTesting(std::u16string path) {
  windows_fonts_path_.swap(path);
}

void DWriteFontProxyImpl::FindFamily(const std::u16string& family_name,
                                     FindFamilyCallback callback) {
  InitializeDirectWrite();
  TRACE_EVENT0("dwrite,fonts", "FontProxyHost::OnFindFamily");
  UINT32 family_index = UINT32_MAX;
  if (collection_) {
    BOOL exists = FALSE;
    UINT32 index = UINT32_MAX;
    HRESULT hr = collection_->FindFamilyName(base::as_wcstr(family_name),
                                             &index, &exists);
    if (SUCCEEDED(hr) && exists &&
        CheckRequiredStylesPresent(collection_.Get(), family_name, index)) {
      family_index = index;
    }
  }
  std::move(callback).Run(family_index);
}

void DWriteFontProxyImpl::GetFamilyCount(GetFamilyCountCallback callback) {
  InitializeDirectWrite();
  TRACE_EVENT0("dwrite,fonts", "FontProxyHost::OnGetFamilyCount");
  std::move(callback).Run(collection_ ? collection_->GetFontFamilyCount() : 0);
}

void DWriteFontProxyImpl::GetFamilyNames(UINT32 family_index,
                                         GetFamilyNamesCallback callback) {
  InitializeDirectWrite();
  TRACE_EVENT0("dwrite,fonts", "FontProxyHost::OnGetFamilyNames");
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::vector<blink::mojom::DWriteStringPairPtr>());
  if (!collection_)
    return;

  TRACE_EVENT0("dwrite,fonts", "FontProxyHost::DoGetFamilyNames");

  mswr::ComPtr<IDWriteFontFamily> family;
  HRESULT hr = collection_->GetFontFamily(family_index, &family);
  if (FAILED(hr)) {
    return;
  }

  mswr::ComPtr<IDWriteLocalizedStrings> localized_names;
  hr = family->GetFamilyNames(&localized_names);
  if (FAILED(hr)) {
    return;
  }

  size_t string_count = localized_names->GetCount();

  std::vector<wchar_t> locale;
  std::vector<wchar_t> name;
  std::vector<blink::mojom::DWriteStringPairPtr> family_names;
  for (size_t index = 0; index < string_count; ++index) {
    UINT32 length = 0;
    hr = localized_names->GetLocaleNameLength(index, &length);
    if (FAILED(hr)) {
      return;
    }
    ++length;  // Reserve space for the null terminator.
    locale.resize(length);
    hr = localized_names->GetLocaleName(index, locale.data(), length);
    if (FAILED(hr)) {
      return;
    }
    CHECK_EQ(L'\0', locale[length - 1]);

    length = 0;
    hr = localized_names->GetStringLength(index, &length);
    if (FAILED(hr)) {
      return;
    }
    ++length;  // Reserve space for the null terminator.
    name.resize(length);
    hr = localized_names->GetString(index, name.data(), length);
    if (FAILED(hr)) {
      return;
    }
    CHECK_EQ(L'\0', name[length - 1]);

    family_names.emplace_back(absl::in_place, base::WideToUTF16(locale.data()),
                              base::WideToUTF16(name.data()));
  }
  std::move(callback).Run(std::move(family_names));
}

void DWriteFontProxyImpl::GetFontFileHandles(
    uint32_t family_index,
    GetFontFileHandlesCallback callback) {
  InitializeDirectWrite();
  TRACE_EVENT0("dwrite,fonts", "FontProxyHost::OnGetFontFiles");
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::vector<base::File>());
  if (!collection_)
    return;

  mswr::ComPtr<IDWriteFontFamily> family;
  HRESULT hr = collection_->GetFontFamily(family_index, &family);
  if (FAILED(hr)) {
    if (IsLastResortFallbackFont(family_index)) {
      LogMessageFilterError(
          MessageFilterError::LAST_RESORT_FONT_GET_FAMILY_FAILED);
    }
    return;
  }

  UINT32 font_count = family->GetFontCount();

  std::set<std::wstring> path_set;
  // Iterate through all the fonts in the family, and all the files for those
  // fonts. If anything goes wrong, bail on the entire family to avoid having
  // a partially-loaded font family.
  for (UINT32 font_index = 0; font_index < font_count; ++font_index) {
    mswr::ComPtr<IDWriteFont> font;
    hr = family->GetFont(font_index, &font);
    if (FAILED(hr)) {
      if (IsLastResortFallbackFont(family_index)) {
        LogMessageFilterError(
            MessageFilterError::LAST_RESORT_FONT_GET_FONT_FAILED);
      }
      return;
    }

    if (FAILED(AddFilesForFont(font.Get(), windows_fonts_path_, &path_set))) {
      if (IsLastResortFallbackFont(family_index)) {
        LogMessageFilterError(
            MessageFilterError::LAST_RESORT_FONT_ADD_FILES_FAILED);
      }
    }
  }

  std::vector<base::File> file_handles;
  // We pass handles for every path as the sandbox blocks direct access to font
  // files in the renderer.
  // TODO(jam): if kDWriteFontProxyOnIO is removed also remove the exception
  // for this class from thread_restrictions.h
  base::ScopedAllowBlocking allow_io;
  for (const auto& font_path : path_set) {
    // Specify FLAG_WIN_EXCLUSIVE_WRITE to prevent base::File from opening the
    // file with FILE_SHARE_WRITE access. FLAG_WIN_EXCLUSIVE_WRITE doesn't
    // actually open the file for write access.
    base::File file(base::FilePath(font_path),
                    base::File::FLAG_OPEN | base::File::FLAG_READ |
                        base::File::FLAG_WIN_EXCLUSIVE_WRITE);
    if (file.IsValid()) {
      file_handles.push_back(std::move(file));
    }
  }
  std::move(callback).Run(std::move(file_handles));
}

void DWriteFontProxyImpl::MapCharacters(
    const std::u16string& text,
    blink::mojom::DWriteFontStylePtr font_style,
    const std::u16string& locale_name,
    uint32_t reading_direction,
    const std::u16string& base_family_name,
    MapCharactersCallback callback) {
  InitializeDirectWrite();
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback),
      blink::mojom::MapCharactersResult::New(
          UINT32_MAX, u"", text.length(), 0.0,
          blink::mojom::DWriteFontStyle::New(DWRITE_FONT_STYLE_NORMAL,
                                             DWRITE_FONT_STRETCH_NORMAL,
                                             DWRITE_FONT_WEIGHT_NORMAL)));
  if (factory2_ == nullptr || collection_ == nullptr)
    return;
  if (font_fallback_ == nullptr) {
    if (FAILED(factory2_->GetSystemFontFallback(&font_fallback_))) {
      return;
    }
  }

  mswr::ComPtr<IDWriteFont> mapped_font;

  mswr::ComPtr<IDWriteNumberSubstitution> number_substitution;
  if (FAILED(factory2_->CreateNumberSubstitution(
          DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, base::as_wcstr(locale_name),
          TRUE /* ignoreUserOverride */, &number_substitution))) {
    DCHECK(false);
    return;
  }
  mswr::ComPtr<IDWriteTextAnalysisSource> analysis_source;
  if (FAILED(gfx::win::TextAnalysisSource::Create(
          &analysis_source, base::UTF16ToWide(text),
          base::UTF16ToWide(locale_name), number_substitution.Get(),
          static_cast<DWRITE_READING_DIRECTION>(reading_direction)))) {
    DCHECK(false);
    return;
  }

  auto result = blink::mojom::MapCharactersResult::New(
      UINT32_MAX, u"", text.length(), 0.0,
      blink::mojom::DWriteFontStyle::New(DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL,
                                         DWRITE_FONT_WEIGHT_NORMAL));
  if (FAILED(font_fallback_->MapCharacters(
          analysis_source.Get(), 0, text.length(), collection_.Get(),
          base::as_wcstr(base_family_name),
          static_cast<DWRITE_FONT_WEIGHT>(font_style->font_weight),
          static_cast<DWRITE_FONT_STYLE>(font_style->font_slant),
          static_cast<DWRITE_FONT_STRETCH>(font_style->font_stretch),
          &result->mapped_length, &mapped_font, &result->scale))) {
    DCHECK(false);
    return;
  }

  if (mapped_font == nullptr) {
    std::move(callback).Run(std::move(result));
    return;
  }

  mswr::ComPtr<IDWriteFontFamily> mapped_family;
  if (FAILED(mapped_font->GetFontFamily(&mapped_family))) {
    DCHECK(false);
    return;
  }
  mswr::ComPtr<IDWriteLocalizedStrings> family_names;
  if (FAILED(mapped_family->GetFamilyNames(&family_names))) {
    DCHECK(false);
    return;
  }

  result->font_style->font_slant = mapped_font->GetStyle();
  result->font_style->font_stretch = mapped_font->GetStretch();
  result->font_style->font_weight = mapped_font->GetWeight();

  std::vector<wchar_t> name;
  size_t name_count = family_names->GetCount();
  for (size_t name_index = 0; name_index < name_count; name_index++) {
    UINT32 name_length = 0;
    if (FAILED(family_names->GetStringLength(name_index, &name_length)))
      continue;  // Keep trying other names

    ++name_length;  // Reserve space for the null terminator.
    name.resize(name_length);
    if (FAILED(family_names->GetString(name_index, name.data(), name_length)))
      continue;
    UINT32 index = UINT32_MAX;
    BOOL exists = false;
    if (FAILED(collection_->FindFamilyName(name.data(), &index, &exists)) ||
        !exists)
      continue;

    // Found a matching family!
    result->family_index = index;
    result->family_name = base::as_u16cstr(name.data());
    std::move(callback).Run(std::move(result));
    return;
  }

  // Could not find a matching family
  LogMessageFilterError(MessageFilterError::MAP_CHARACTERS_NO_FAMILY);
  DCHECK_EQ(result->family_index, UINT32_MAX);
  DCHECK_GT(result->mapped_length, 0u);
}

void DWriteFontProxyImpl::MatchUniqueFont(
    const std::u16string& unique_font_name,
    MatchUniqueFontCallback callback) {
  TRACE_EVENT0("dwrite,fonts", "DWriteFontProxyImpl::MatchUniqueFont");

  DCHECK(base::FeatureList::IsEnabled(features::kFontSrcLocalMatching));
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                         base::File(), 0);
  InitializeDirectWrite();

  // We must not get here if this version of DWrite can't handle performing the
  // search.
  DCHECK(factory3_.Get());
  DCHECK(collection_);
  Microsoft::WRL::ComPtr<IDWriteFontCollection1> collection1;
  HRESULT hr = collection_.As(&collection1);
  if (FAILED(hr)) {
    return;
  }
  // In non-testing cases this is identical to factory3_->GetSystemFontSet().
  mswr::ComPtr<IDWriteFontSet> system_font_set;
  hr = collection1->GetFontSet(&system_font_set);
  if (FAILED(hr))
    return;

  DCHECK_GT(system_font_set->GetFontCount(), 0U);

  mswr::ComPtr<IDWriteFontSet> filtered_set;

  auto filter_set = [&system_font_set, &filtered_set,
                     &unique_font_name](DWRITE_FONT_PROPERTY_ID property_id) {
    TRACE_EVENT0("dwrite,fonts",
                 "DWriteFontProxyImpl::MatchUniqueFont::filter_set");
    std::wstring unique_font_name_wide = base::UTF16ToWide(unique_font_name);
    DWRITE_FONT_PROPERTY search_property = {property_id,
                                            unique_font_name_wide.c_str(), L""};
    // GetMatchingFonts() matches all languages according to:
    // https://docs.microsoft.com/en-us/windows/desktop/api/dwrite_3/ns-dwrite_3-dwrite_font_property
    HRESULT hr =
        system_font_set->GetMatchingFonts(&search_property, 1, &filtered_set);
    return SUCCEEDED(hr);
  };

  // Search PostScript name first, otherwise try searching for full font name.
  // Return if filtering failed.
  if (!filter_set(DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME))
    return;

  if (!filtered_set->GetFontCount() &&
      !filter_set(DWRITE_FONT_PROPERTY_ID_FULL_NAME)) {
    return;
  }

  if (!filtered_set->GetFontCount())
    return;

  mswr::ComPtr<IDWriteFontFaceReference> first_font;
  hr = filtered_set->GetFontFaceReference(0, &first_font);
  if (FAILED(hr))
    return;

  mswr::ComPtr<IDWriteFontFace3> first_font_face_3;
  hr = first_font->CreateFontFace(&first_font_face_3);
  if (FAILED(hr))
    return;

  mswr::ComPtr<IDWriteFontFace> first_font_face;
  hr = first_font_face_3.As<IDWriteFontFace>(&first_font_face);
  if (FAILED(hr))
    return;

  std::wstring font_file_pathname;
  uint32_t ttc_index;
  if (FAILED(FontFilePathAndTtcIndex(first_font_face.Get(), font_file_pathname,
                                     ttc_index))) {
    return;
  }

  base::FilePath path(font_file_pathname);

  // Have the Browser process open the font file and send the handle to the
  // Renderer Process to access the font. Otherwise, user-installed local font
  // files outside of Windows fonts system directory wouldn't be accessible by
  // Renderer due to Windows sandboxing rules.

  // Specify FLAG_WIN_EXCLUSIVE_WRITE to prevent base::File from opening the
  // file with FILE_SHARE_WRITE access. FLAG_WIN_EXCLUSIVE_WRITE doesn't
  // actually open the file for write access.
  base::File font_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                 base::File::FLAG_WIN_EXCLUSIVE_WRITE);
  if (!font_file.IsValid() || !font_file.GetLength()) {
    return;
  }
  std::move(callback).Run(std::move(font_file), ttc_index);
}

void DWriteFontProxyImpl::FallbackFamilyAndStyleForCodepoint(
    const std::string& base_family_name,
    const std::string& locale_name,
    uint32_t codepoint,
    FallbackFamilyAndStyleForCodepointCallback callback) {
  InitializeDirectWrite();
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback),
      blink::mojom::FallbackFamilyAndStyle::New("",
                                                /* weight */ 0,
                                                /* width */ 0,
                                                /* slant */ 0));

  if (!codepoint || !collection_ || !factory_)
    return;

  sk_sp<SkFontMgr> font_mgr(
      SkFontMgr_New_DirectWrite(factory_.Get(), collection_.Get()));

  if (!font_mgr)
    return;

  const char* bcp47_locales[] = {locale_name.c_str()};
  int num_locales = locale_name.empty() ? 0 : 1;
  const char** locales = locale_name.empty() ? nullptr : bcp47_locales;

  sk_sp<SkTypeface> typeface(font_mgr->matchFamilyStyleCharacter(
      base_family_name.c_str(), SkFontStyle(), locales, num_locales,
      codepoint));

  if (!typeface)
    return;

  SkString family_name;
  typeface->getFamilyName(&family_name);

  SkFontStyle font_style = typeface->fontStyle();

  auto result_fallback_and_style = blink::mojom::FallbackFamilyAndStyle::New(
      family_name.c_str(), font_style.weight(), font_style.width(),
      font_style.slant());
  std::move(callback).Run(std::move(result_fallback_and_style));
}

void DWriteFontProxyImpl::InitializeDirectWrite() {
  if (direct_write_initialized_)
    return;
  direct_write_initialized_ = true;

  TRACE_EVENT0("dwrite,fonts", "DWriteFontProxyImpl::InitializeDirectWrite");

  gfx::win::CreateDWriteFactory(&factory_);
  if (factory_ == nullptr) {
    // We won't be able to load fonts, but we should still return messages so
    // renderers don't hang if they for some reason send us a font message.
    return;
  }

  // QueryInterface for IDWriteFactory2. This should succeed since we only
  // support >= Win10.
  factory_.As<IDWriteFactory2>(&factory2_);
  DCHECK(factory2_);

  // QueryInterface for IDwriteFactory3, needed for MatchUniqueFont on Windows.
  // This should succeed since we only support >= Win10.
  factory_.As<IDWriteFactory3>(&factory3_);
  DCHECK(factory3_);

  // Normally identical to factory_->GetSystemFontCollection() unless a
  // sideloaded font has been added using SideLoadFontForTesting().
  HRESULT hr = GetLocalFontCollection(factory3_, &collection_);
  DCHECK(SUCCEEDED(hr));

  if (!collection_) {
    base::UmaHistogramSparse(
        "DirectWrite.Fonts.Proxy.GetSystemFontCollectionResult", hr);
    LogMessageFilterError(MessageFilterError::ERROR_NO_COLLECTION);
    return;
  }

  // Temp code to help track down crbug.com/561873
  for (size_t font = 0; font < std::size(kLastResortFontNames); font++) {
    uint32_t font_index = 0;
    BOOL exists = FALSE;
    if (SUCCEEDED(collection_->FindFamilyName(kLastResortFontNames[font],
                                              &font_index, &exists)) &&
        exists && font_index != UINT32_MAX) {
      last_resort_fonts_.push_back(font_index);
    }
  }
  LogLastResortFontCount(last_resort_fonts_.size());
}

bool DWriteFontProxyImpl::IsLastResortFallbackFont(uint32_t font_index) {
  for (auto iter = last_resort_fonts_.begin(); iter != last_resort_fonts_.end();
       ++iter) {
    if (*iter == font_index)
      return true;
  }
  return false;
}

}  // namespace content
