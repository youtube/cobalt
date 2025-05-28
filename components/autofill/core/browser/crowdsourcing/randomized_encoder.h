// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_RANDOMIZED_ENCODER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_RANDOMIZED_ENCODER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/signatures.h"

class PrefService;

namespace autofill {

// Encodes string values using the differential-privacy scheme as described
// in go/autofill-metadata-upload (Google internal link).
class RandomizedEncoder {
 public:
  struct EncodingInfo {
    AutofillRandomizedValue_EncodingType encoding_type;
    size_t chunk_length_in_bytes;
    size_t bit_offset;
    size_t bit_stride;
  };

  // Form-level data-type identifiers.
  static constexpr char kFormId[] = "form-id";
  static constexpr char kFormName[] = "form-name";
  static constexpr char kFormAction[] = "form-action";
  static constexpr char kFormUrl[] = "form-url";
  static constexpr char kFormCssClass[] = "form-css-class";
  static constexpr char kFormButtonTitles[] = "button-titles";

  // Field-level data-type identifiers.
  static constexpr char kFieldId[] = "field-id";
  static constexpr char kFieldName[] = "field-name";
  static constexpr char kFieldControlType[] = "field-control-type";
  static constexpr char kFieldLabel[] = "field-label";
  static constexpr char kFieldAriaLabel[] = "field-aria-label";
  static constexpr char kFieldAriaDescription[] = "field-aria-description";
  static constexpr char kFieldCssClasses[] = "field-css-classes";
  static constexpr char kFieldPlaceholder[] = "field-placeholder";
  static constexpr char kFieldInitialValueHash[] = "field-initial-hash-value";
  static constexpr char kFieldAutocomplete[] = "field-autocomplete";
  static constexpr char kFieldPattern[] = "field-pattern";

  // Copy of components/unified_consent/pref_names.cc
  // We could not use the constant from components/unified_constants because of
  // a circular dependency.
  // TODO(crbug.com/40570965): resolve circular dependency and remove
  // hardcoded constant
  static constexpr char kUrlKeyedAnonymizedDataCollectionEnabled[] =
      "url_keyed_anonymized_data_collection.enabled";

  // Factory Function
  static std::unique_ptr<RandomizedEncoder> Create(PrefService* pref_service);

  RandomizedEncoder(std::string seed,
                    AutofillRandomizedValue_EncodingType encoding_type,
                    bool anonymous_url_collection_is_enabled);

  // Encode |data_value| using this instance's |encoding_type_|.
  // If |data_type!=FORM_URL|, the output value's length is limited by
  // |kEncodedChunkLengthInBytes|.
  std::string Encode(FormSignature form_signature,
                     FieldSignature field_signature,
                     std::string_view data_type,
                     std::string_view data_value) const;
  // Used for testing, converts |data_value| to UTF-8 and calls Encode().
  std::string EncodeForTesting(FormSignature form_signature,
                               FieldSignature field_signature,
                               std::string_view data_type,
                               std::u16string_view data_value) const;

  AutofillRandomizedValue_EncodingType encoding_type() const {
    DCHECK(encoding_info_);
    return encoding_info_
               ? encoding_info_->encoding_type
               : AutofillRandomizedValue_EncodingType_UNSPECIFIED_ENCODING_TYPE;
  }
  bool AnonymousUrlCollectionIsEnabled() const {
    return anonymous_url_collection_is_enabled_;
  }

 protected:
  // Get the pseudo-random string to use at the coin bit-field. This function
  // is internal, but exposed here to facilitate testing.
  std::string GetCoins(FormSignature form_signature,
                       FieldSignature field_signature,
                       std::string_view data_type,
                       int encoding_length) const;

  // Get the pseudo-random string to use at the noise bit-field. This function
  // is internal, but exposed here to facilitate testing.
  std::string GetNoise(FormSignature form_signature,
                       FieldSignature field_signature,
                       std::string_view data_type,
                       int encoding_length) const;

  // For |data_type==FORM_URL|, returns required chunk count to fit
  // |data_value|, but max |kMaxChunks|. Otherwise, returns 1.
  int GetChunkCount(std::string_view data_value,
                    std::string_view data_type) const;

 private:
  const std::string seed_;
  const raw_ptr<const EncodingInfo> encoding_info_;
  const bool anonymous_url_collection_is_enabled_;
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_RANDOMIZED_ENCODER_H_
