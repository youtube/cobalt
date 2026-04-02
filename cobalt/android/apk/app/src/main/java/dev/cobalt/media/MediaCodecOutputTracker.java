// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
//
// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package dev.cobalt.media;

private class OutputMemoryTracker {
    private final SparseIntArray mBufferSizes = new SparseIntArray();
    private long mTotalMemoryUsage = 0;

    public synchronized void add(int index, int size) {
      int oldSize = mBufferSizes.get(index, -1);
      if (oldSize != -1) {
        mTotalMemoryUsage -= oldSize;
        sGlobalTotalMemoryUsage.addAndGet(-oldSize);
      }
      int trackedSize = size;
      if (mDecodesToSurface) {
        try {
          if (mActiveFormat != null) {
            int width = mActiveFormat.width();
            int height = mActiveFormat.height();
            if (width > 0 && height > 0) {
              // This is an estimated size assuming YUV420 format
              // width * height * 1.5
              trackedSize = width * height * 3 / 2;
            }
          }
        } catch (Exception e) {
          trackedSize = 0;
        }
      }
      mBufferSizes.put(index, trackedSize);
      mTotalMemoryUsage += trackedSize;
      sGlobalTotalMemoryUsage.addAndGet(trackedSize);
    }

    public synchronized void remove(int index) {
      int size = mBufferSizes.get(index, -1);
      if (size != -1) {
        mTotalMemoryUsage -= size;
        sGlobalTotalMemoryUsage.addAndGet(-size); 
        mBufferSizes.delete(index);
      }
    }

    public synchronized void reset() {
      mBufferSizes.clear();
      sGlobalTotalMemoryUsage.addAndGet(-mTotalMemoryUsage);
      mTotalMemoryUsage = 0;
    }
  }