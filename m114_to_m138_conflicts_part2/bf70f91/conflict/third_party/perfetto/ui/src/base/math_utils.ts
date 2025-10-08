// Copyright (C) 2023 The Android Open Source Project
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
// Restrict the value of a number between two values (inclusive)
export function clamp(val: number, lower: number, upper: number): number {
  return Math.max(lower, Math.min(upper, val));
}

const FLOAT_EPSILON = 1e-9;

// Returns true if two floats are close enough to be considered equal.
export function floatEqual(a: number, b: number, eps = FLOAT_EPSILON): boolean {
  return Math.abs(a - b) < eps;
=======
// Round a number up to the nearest stepsize.
export function roundUpNearest(val: number, stepsize: number): number {
  return stepsize * Math.ceil(val / stepsize);
}

// Round a number down to the nearest stepsize.
export function roundDownNearest(val: number, stepsize: number): number {
  return stepsize * Math.floor(val / stepsize);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
}
