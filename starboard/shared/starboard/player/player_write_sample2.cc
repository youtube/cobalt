// Copyright 2018 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/player.h"

#include "starboard/common/log.h"
#include "starboard/shared/starboard/player/player_internal.h"

void SbPlayerWriteSample2(SbPlayer player,
                          SbMediaType sample_type,
                          const SbPlayerSampleInfo* sample_infos,
                          int number_of_sample_infos) {
  if (!SbPlayerIsValid(player)) {
    SB_LOG(WARNING) << "player is invalid.";
    return;
  }
  if (!sample_infos) {
    SB_LOG(WARNING) << "sample_infos is null.";
    return;
  }
  if (number_of_sample_infos < 1) {
    SB_LOG(WARNING) << "number_of_sample_infos is " << number_of_sample_infos
                    << ", which should be greater than or equal to 1";
    return;
  }
  auto max_samples_per_write =
      SbPlayerGetMaximumNumberOfSamplesPerWrite(player, sample_type);
  if (number_of_sample_infos > max_samples_per_write) {
    SB_LOG(WARNING) << "number_of_sample_infos is " << number_of_sample_infos
                    << ", which should be less than or equal to "
                    << max_samples_per_write;
  }

  player->WriteSamples(sample_infos, number_of_sample_infos);
}
