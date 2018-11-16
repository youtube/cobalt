// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines StructTraits specializations for translating between mojo types and
// metrics:: types, with data validity checks.

#ifndef COMPONENTS_METRICS_PUBLIC_CPP_CALL_STACK_PROFILE_STRUCT_TRAITS_H_
#define COMPONENTS_METRICS_PUBLIC_CPP_CALL_STACK_PROFILE_STRUCT_TRAITS_H_

#include <string>

#include "base/strings/string_piece.h"
#include "components/metrics/public/interfaces/call_stack_profile_collector.mojom.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace mojo {

template <>
struct StructTraits<metrics::mojom::SampledProfileDataView,
                    metrics::SampledProfile> {
  static std::string contents(const metrics::SampledProfile& profile) {
    std::string output;
    profile.SerializeToString(&output);
    return output;
  }

  static bool Read(metrics::mojom::SampledProfileDataView data,
                   metrics::SampledProfile* out) {
    base::StringPiece contents;
    if (!data.ReadContents(&contents))
      return false;

    if (!out->ParseFromArray(contents.data(), contents.size()))
      return false;

    // This is purely a sanity check to minimize bad data uploaded, and not
    // required for security reasons.
    if (!out->unknown_fields().empty())
      return false;

    return true;
  }
};

}  // namespace mojo

#endif  // COMPONENTS_METRICS_PUBLIC_CPP_CALL_STACK_PROFILE_STRUCT_TRAITS_H_
