// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RESULT_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RESULT_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "third_party/chromium/media/mojo/mojom/speech_recognition_result.h"
#include "third_party/chromium/media/mojo/mojom/speech_recognition_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

template <>
class StructTraits<media_m96::mojom::HypothesisPartsDataView,
                   media_m96::HypothesisParts> {
 public:
  static const std::vector<std::string>& text(const media_m96::HypothesisParts& r) {
    return r.text;
  }

  static base::TimeDelta hypothesis_part_offset(
      const media_m96::HypothesisParts& r) {
    return r.hypothesis_part_offset;
  }

  static bool Read(media_m96::mojom::HypothesisPartsDataView data,
                   media_m96::HypothesisParts* out);
};

template <>
class StructTraits<media_m96::mojom::TimingInformationDataView,
                   media_m96::TimingInformation> {
 public:
  static base::TimeDelta audio_start_time(const media_m96::TimingInformation& r) {
    return r.audio_start_time;
  }

  static base::TimeDelta audio_end_time(const media_m96::TimingInformation& r) {
    return r.audio_end_time;
  }

  static const ::absl::optional<std::vector<media_m96::HypothesisParts>>&
  hypothesis_parts(const media_m96::TimingInformation& r) {
    return r.hypothesis_parts;
  }

  static bool Read(media_m96::mojom::TimingInformationDataView data,
                   media_m96::TimingInformation* out);
};

template <>
class StructTraits<media_m96::mojom::SpeechRecognitionResultDataView,
                   media_m96::SpeechRecognitionResult> {
 public:
  static const std::string& transcription(
      const media_m96::SpeechRecognitionResult& r) {
    return r.transcription;
  }

  static bool is_final(const media_m96::SpeechRecognitionResult& r) {
    return r.is_final;
  }

  static const ::absl::optional<media_m96::TimingInformation>& timing_information(
      const media_m96::SpeechRecognitionResult& r) {
    return r.timing_information;
  }

  static bool Read(media_m96::mojom::SpeechRecognitionResultDataView data,
                   media_m96::SpeechRecognitionResult* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RESULT_MOJOM_TRAITS_H_
