/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/memory_settings/constrainer.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "cobalt/browser/memory_settings/memory_settings.h"
#include "starboard/configuration.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {

// Any memory setting that matches the MemoryType and is an AutoSet type is
// passed to the output.
std::vector<MemorySetting*> FilterSettings(
    MemorySetting::MemoryType memory_type,
    const std::vector<MemorySetting*>& settings) {
  std::vector<MemorySetting*> output;
  for (size_t i = 0; i < settings.size(); ++i) {
    MemorySetting* setting = settings[i];
    if (setting->memory_type() == memory_type) {
      output.push_back(setting);
    }
  }
  return output;
}

// Sums the memory consumption at the given global_constraining_value. The
// settings variable is read buy not modified (despite the non-const
// declaration). If constrained_values_out is non-null, then the computed
// constraining values are stored in this vector.
// Returns: The amount of memory in bytes that the memory settings vector will
//          consume at the given global_constraining_factor.
int64_t SumMemoryConsumption(
    double global_constraining_factor,
    const std::vector<MemorySetting*>& memory_settings,
    std::vector<double>* constrained_values_out) {

  if (constrained_values_out) {
    constrained_values_out->clear();
  }

  int64_t sum = 0;

  // Iterates through the MemorySettings and determines the total memory
  // consumption at the current global_constraining_value.
  for (size_t i = 0; i < memory_settings.size(); ++i) {
    const MemorySetting* setting = memory_settings[i];

    const int64_t requested_consumption = setting->MemoryConsumption();
    double local_constraining_value = 1.0;
    if (setting->source_type() == MemorySetting::kAutoSet) {
      local_constraining_value =
          setting->ComputeAbsoluteMemoryScale(global_constraining_factor);
    }

    const int64_t new_consumption_value =
        static_cast<int64_t>(local_constraining_value * requested_consumption);

    if (constrained_values_out) {
      constrained_values_out->push_back(local_constraining_value);
    }

    sum += new_consumption_value;
  }

  return sum;
}

void CheckMemoryChange(const std::string& setting_name,
                       int64_t old_memory_consumption,
                       int64_t new_memory_consumption,
                       double constraining_value) {
  // Represents 1% allowed difference.
  static const double kErrorThreshold = 0.01;

  const double actual_constraining_value =
      static_cast<double>(new_memory_consumption) /
      static_cast<double>(old_memory_consumption);

  double diff = constraining_value - actual_constraining_value;
  if (diff < 0.0) {
    diff = -diff;
  }

  DCHECK_LE(diff, kErrorThreshold)
      << "MemorySetting " << setting_name << " did not change it's memory by "
      << "the expected value.\n"
      << "  Expected Change: " << (constraining_value * 100) << "%\n"
      << "  Actual Change: " << (diff * 100) << "%\n"
      << "  Original memory consumption (bytes): " << old_memory_consumption
      << "  New memory consumption (bytes):      " << new_memory_consumption
      << "\n";
}

void ConstrainToMemoryLimit(int64_t memory_limit,
                            std::vector<MemorySetting*>* memory_settings) {
  if (memory_settings->empty()) {
    return;
  }

  // If the memory consumed is already under the memory limit then no further
  // work needs to be done.
  if (SumMemoryConsumption(1.0, *memory_settings, NULL) <= memory_limit) {
    return;
  }

  // Iterate by small steps the constraining value from 1.0 (100%) toward
  // 0.0.
  static const double kStep = 0.0001;  // .01% change per iteration.
  std::vector<double> constrained_sizes;
  // 1-1 mapping.
  constrained_sizes.resize(memory_settings->size());
  for (double global_constraining_factor = 1.0;
       global_constraining_factor >= 0.0;
       global_constraining_factor -= kStep) {
    global_constraining_factor = std::max(0.0, global_constraining_factor);
    const int64_t new_global_memory_consumption =
        SumMemoryConsumption(global_constraining_factor, *memory_settings,
                             &constrained_sizes);

    const bool finished =
        (global_constraining_factor == 0.0) ||
        (new_global_memory_consumption <= memory_limit);

    if (finished) {
      break;
    }
  }
  DCHECK_EQ(memory_settings->size(), constrained_sizes.size());
  for (size_t i = 0; i < memory_settings->size(); ++i) {
    const double local_constraining_factor = constrained_sizes[i];
    MemorySetting* setting = memory_settings->at(i);
    if (local_constraining_factor != 1.0) {
      const int64_t old_memory_consumption = setting->MemoryConsumption();
      DCHECK_EQ(setting->source_type(), MemorySetting::kAutoSet);
      setting->ScaleMemory(local_constraining_factor);
      const int64_t new_memory_consumption = setting->MemoryConsumption();

      // If the memory doesn't actually change as predicted by the constraining
      // value then this check will catch it here.
      CheckMemoryChange(setting->name(),
                        old_memory_consumption, new_memory_consumption,
                        local_constraining_factor);
    }
  }
}

}  // namespace.

void ConstrainToMemoryLimits(
    const int64_t& max_cpu_memory,
    const base::optional<int64_t>& max_gpu_memory,
    std::vector<MemorySetting*>* memory_settings,
    std::vector<std::string>* error_msgs) {

  // Some platforms may just return 0 for the max_cpu memory, in this case
  // we don't want to constrain the memory.
  if (max_cpu_memory > 0) {
    std::vector<MemorySetting*> cpu_memory_settings =
        FilterSettings(MemorySetting::kCPU, *memory_settings);
    ConstrainToMemoryLimit(max_cpu_memory, &cpu_memory_settings);
  } else {
    error_msgs->push_back(
        "ERROR - COULD NOT CONSTRAIN CPU MEMORY "
        "BECAUSE max_cobalt_cpu_usage WAS 0.");
  }

  // Some platforms may just return 0 for the max_gpu memory, in this case
  // we don't want to constrain the memory.
  if (max_gpu_memory) {
    if (*max_gpu_memory > 0) {
      std::vector<MemorySetting*> gpu_memory_settings =
          FilterSettings(MemorySetting::kGPU, *memory_settings);
      ConstrainToMemoryLimit(*max_gpu_memory, &gpu_memory_settings);
    } else {
      error_msgs->push_back(
          "ERROR - COULD NOT CONSTRAIN GPU MEMORY "
          "BECAUSE max_cobalt_gpu_usage WAS 0.");
    }
  }
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
