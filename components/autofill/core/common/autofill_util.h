// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_UTIL_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "url/gurl.h"

namespace autofill {

// The length of the GUIDs used for local autofill data. It is different than
// the length used for server autofill data.
constexpr int kLocalGuidSize = 36;

// Returns true when command line switch |kEnableSuggestionsWithSubstringMatch|
// is on.
bool IsFeatureSubstringMatchEnabled();

// Returns true if showing autofill signature as HTML attributes is enabled.
bool IsShowAutofillSignaturesEnabled();

// Returns true when keyboard accessory is enabled.
bool IsKeyboardAccessoryEnabled();

// A token is a sequences of contiguous characters separated by any of the
// characters that are part of delimiter set {' ', '.', ',', '-', '_', '@'}.

// Returns true if the |field_contents| is a substring of the |suggestion|
// starting at token boundaries. |field_contents| can span multiple |suggestion|
// tokens.
bool FieldIsSuggestionSubstringStartingOnTokenBoundary(
    const std::u16string& suggestion,
    const std::u16string& field_contents,
    bool case_sensitive);

// Currently, a token for the purposes of this method is defined as {'@'}.
// Returns true if the |full_string| has a |prefix| as a prefix and the prefix
// ends on a token.
bool IsPrefixOfEmailEndingWithAtSign(const std::u16string& full_string,
                                     const std::u16string& prefix);

// Finds the first occurrence of a searched substring |field_contents| within
// the string |suggestion| starting at token boundaries and returns the index to
// the end of the located substring, or std::u16string::npos if the substring is
// not found. "preview-on-hover" feature is one such use case where the
// substring |field_contents| may not be found within the string |suggestion|.
size_t GetTextSelectionStart(const std::u16string& suggestion,
                             const std::u16string& field_contents,
                             bool case_sensitive);

bool IsCheckable(const FormFieldData::CheckStatus& check_status);
bool IsChecked(const FormFieldData::CheckStatus& check_status);
void SetCheckStatus(FormFieldData* form_field_data,
                    bool isCheckable,
                    bool isChecked);

// Lowercases and tokenizes a given |attribute| string.
// Considers any ASCII whitespace character as a possible separator.
// Also ignores empty tokens, resulting in a collapsing of whitespace.
std::vector<std::string> LowercaseAndTokenizeAttributeString(
    base::StringPiece attribute);

// Returns true if and only if the field value has no character except the
// formatting characters. This means that the field value is a formatting string
// entered by the website and not a real value entered by the user.
bool SanitizedFieldIsEmpty(const std::u16string& value);

// Returns true if the first suggestion should be autoselected when the autofill
// dropdown is shown due to an arrow down event. Enabled on desktop only.
bool ShouldAutoselectFirstSuggestionOnArrowDown();

// Returns true if focused_field_type corresponds to a fillable field.
bool IsFillable(mojom::FocusedFieldType focused_field_type);

mojom::SubmissionIndicatorEvent ToSubmissionIndicatorEvent(
    mojom::SubmissionSource source);

// Strips any authentication data, as well as query and ref portions of URL.
GURL StripAuthAndParams(const GURL& gurl);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_UTIL_H_
