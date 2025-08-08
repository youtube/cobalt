// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_CPU_USAGE_DATA_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_CPU_USAGE_DATA_H_

#include <cstdint>
#include <limits>

namespace app_list {

/* This class is used to store and calculate the system and user usage. The
delta between the current previous values is used to calculate the final cpu
usage values which are stored in CPU data.*/
class CpuUsageData {
 public:
  CpuUsageData() = default;
  CpuUsageData(uint64_t user_time, uint64_t system_time, uint64_t idle_time);

  ~CpuUsageData() = default;

  CpuUsageData(const CpuUsageData& other) = default;
  CpuUsageData& operator=(const CpuUsageData& other) = default;

  bool IsInitialized() const;

  uint64_t GetUserTime() const { return user_time_; }

  uint64_t GetSystemTime() const { return system_time_; }

  uint64_t GetIdleTime() const { return idle_time_; }

  uint64_t GetTotalTime() const;

  CpuUsageData operator+(const CpuUsageData& other) const;
  CpuUsageData& operator+=(const CpuUsageData& other);
  CpuUsageData operator-(const CpuUsageData& other) const;
  CpuUsageData& operator-=(const CpuUsageData& other);

 private:
  uint64_t user_time_ = std::numeric_limits<uint64_t>::max();
  uint64_t system_time_ = std::numeric_limits<uint64_t>::max();
  uint64_t idle_time_ = std::numeric_limits<uint64_t>::max();
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_CPU_USAGE_DATA_H_
