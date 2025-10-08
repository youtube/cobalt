// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

<<<<<<< HEAD
import {raf} from '../core/raf_scheduler';
=======
import {globals} from './globals';
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

export class Animation {
  private startMs = 0;
  private endMs = 0;
  private boundOnAnimationFrame = this.onAnimationFrame.bind(this);

  constructor(private onAnimationStep: (timeSinceStartMs: number) => void) {}

  start(durationMs: number) {
    const nowMs = performance.now();

    // If the animation is already happening, just update its end time.
    if (nowMs <= this.endMs) {
      this.endMs = nowMs + durationMs;
      return;
    }
    this.startMs = nowMs;
    this.endMs = nowMs + durationMs;
<<<<<<< HEAD
    raf.startAnimation(this.boundOnAnimationFrame);
=======
    globals.rafScheduler.start(this.boundOnAnimationFrame);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }

  stop() {
    this.endMs = 0;
<<<<<<< HEAD
    raf.stopAnimation(this.boundOnAnimationFrame);
=======
    globals.rafScheduler.stop(this.boundOnAnimationFrame);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }

  get startTimeMs(): number {
    return this.startMs;
  }

  private onAnimationFrame(nowMs: number) {
    if (nowMs >= this.endMs) {
<<<<<<< HEAD
      raf.stopAnimation(this.boundOnAnimationFrame);
=======
      globals.rafScheduler.stop(this.boundOnAnimationFrame);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      return;
    }
    this.onAnimationStep(Math.max(Math.round(nowMs - this.startMs), 0));
  }
}
