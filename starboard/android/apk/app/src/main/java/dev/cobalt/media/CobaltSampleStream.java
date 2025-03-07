// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.media;

import androidx.media3.common.C;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.decoder.DecoderInputBuffer;
import androidx.media3.exoplayer.FormatHolder;
import androidx.media3.exoplayer.source.SampleStream;
import java.io.IOException;

@UnstableApi
public class CobaltSampleStream implements SampleStream {

  @Override
  public boolean isReady() {
    return false;
  }

  @Override
  public void maybeThrowError() throws IOException {

  }

  @Override
  public int readData(FormatHolder formatHolder, DecoderInputBuffer buffer, int readFlags) {
    return C.RESULT_NOTHING_READ;
  }

  @Override
  public int skipData(long positionUs) {
    return 0;
  }
}
