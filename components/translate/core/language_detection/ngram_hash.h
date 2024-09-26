// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_NGRAM_HASH_H_
#define COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_NGRAM_HASH_H_

#include "third_party/tflite/src/tensorflow/lite/kernels/register.h"

namespace translate {

// This op takes in a string, finds the character ngrams for it and then
// maps each of these ngrams to an index using the specified vocabulary sizes.
TfLiteRegistration* Register_NGRAM_HASH();

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_NGRAM_HASH_H_
