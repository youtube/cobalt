/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/loader/loader.h"

namespace cobalt {
namespace loader {

Loader::Loader(
    base::Callback<scoped_ptr<Fetcher>(Fetcher::Handler*)> fetcher_creator,
    scoped_ptr<Decoder> decoder, ErrorCallback error_callback)
    : decoder_(decoder.Pass()),
      decoder_to_fetcher_adaptor_(make_scoped_ptr(
          new DecoderToFetcherAdapter(decoder_.get(), error_callback))),
      fetcher_(fetcher_creator.Run(decoder_to_fetcher_adaptor_.get()).Pass()) {}

}  // namespace loader
}  // namespace cobalt
