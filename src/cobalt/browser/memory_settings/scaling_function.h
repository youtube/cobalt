/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_SCALING_FUNCTION_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_SCALING_FUNCTION_H_

#include <algorithm>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

typedef base::Callback<double(double)> ScalingFunction;

// The ConstrainerFunction generates constraining values which are used to
// to adjust the memory settings from an input "requested_memory_scale".
//
// "requested_memory_scale" requests memory be scaled by a certain amount
// percentage where 0.0 means "eliminate all memory please" and 1.0 is
// "don't change" and 2.0 means "expand your memory consumption by 2x".
//
// A ScalingFunction works by remapping the input
// "requested_memory_scale" value to an output "memory scaling factor" which
// indicates how much memory will actually be reduced at the current scale.
// This output ranges from ranges from 1.0 (no scaling - 100% of memory is
// retained) to 0.0 (all memory is eliminated from this setting).
//
// Another way to understand the memory_scale value is by the
// following analogy:
//  when (1.0 > input => .9) => "Please reduce memory a little"
//  when (.9 > input >= .5)  => "Please reduce memory - serious!"
//  when (.5 > input >= .25) => "Please reduce memory - super serious!!"
//  when (.25 > input >= .0) => "Please reduce memory - last chances!!!"
//
// Similarly:
//  when (2.0 > input >= 1) => "Please expand memory by a lot.".
//
// Output "memory scaling factor" value represents the amount of memory that
// will be scaled at the current requested_memory_scale value. This
// "memory scaling factor" is an absolute value, such that 0.5 means that
// the memory consumption will be reduced by 50%.
//
// EXAMPLE:
//   Function(.9) => Returns => 0.1
//   Means: "You asked me to reduce memory a little, but I can go ahead and
//          free up 90% of my memory."
//
//   Function(.5) => Returns => 1.0
//   Means: "You asked me to reduce memory and were serious, but I am refusing
//          to reduce memory consumption."
//
//   Function(.25) => Returns => .9
//   Means: "You are at the final stages of asking me to reduce memory and
//          I can go ahead and reduce my memory foot print by 10%."
//
// More information:
//
// Low Priority Memory Settings should respond quickly to constraining
// situations by quickly shedding memory:
//
//    1 *+--------+---------+---------+---------+--------++
//      +*        +         +         +         +         +
//      | *                                               |
//      |  *                                              |
//  0.8 ++  *                                            ++
//      |    *                                            |
//      |     *                                           |
//      |      **                                         |
//  0.6 ++      **     y=x*x*x                           ++
//      |        **                                       |
//      |          **                                     |
//  0.4 ++          **                                   ++
//      |             **                                  |
//      |              ***                                |
//      |                ***                              |
//  0.2 ++                 ***                           ++
//      |                     ***                         |
//      |                        ****                     |
//      +         +         +        *******    +         +
//    0 ++--------+---------+---------+----****************
//      1        0.8       0.6       0.4       0.2        0
//
//
// This is a curve of a high priority setting: one which is resistant to giving
// up memory.
//    1 ******----+---------+---------+---------+--------++
//      +     *********     +         +         +         +
//      |             ********                            |
//      |                     *******                     |
//  0.8 ++                          ******               ++
//      |               y=sqrt(sqrt(x))  *****            |
//      |                                    ****         |
//      |                                        ***      |
//  0.6 ++                                         ***   ++
//      |                                            **   |
//      |                                              ** |
//  0.4 ++                                              *++
//      |                                                *|
//      |                                                *|
//      |                                                 *
//  0.2 ++                                               +*
//      |                                                 *
//      |                                                 *
//      +         +         +         +         +         *
//    0 ++--------+---------+---------+---------+--------+*
//      1        0.8       0.6       0.4       0.2        0
//
// While the following graph will refuse to give up any memory whatsoever.
// For example.
//                                 y=1
//    1.0 *************************************************
//        +         +        +         +        +       * +
//        |                                               |
//        |                                               |
//        |                                               |
//        |                                               |
//        |                                               |
//        |                                               |
//        |                                               |
//    0.5 |                                               |
//        |                                               |
//        |                                               |
//        |                                               |
//        |                                               |
//        |                                               |
//        |                                               |
//        |                                               |
//        |                                               |
//        +         +        +         +        +         +
//   0.0  ++--------+--------+---------+--------+--------++
//        1        0.8      0.6       0.4      0.2        0
//

// Generates a functor that will lower the memory consumption linearly with
// the progress of the input, but will clamp at the input level.
// Example:
//   ConstrainerFunction fun = MakeLinearConstrainerFunction(.75);
//   EXPECT_EQ(.9, fun(.9));
//   EXPECT_EQ(.75, fun(.75));
//   EXPECT_EQ(.75, fun(.5));
//   EXPECT_EQ(.75, fun(0));
ScalingFunction MakeLinearMemoryScaler(double min_clamp_value,
                                       double max_clamp_value);

// Generates a functor that will shed 50% of memory when the requested
// memory scale is .5 or less, otherwise no memory is reduced.
ScalingFunction MakeSkiaGlyphAtlasMemoryScaler();

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_SCALING_FUNCTION_H_
