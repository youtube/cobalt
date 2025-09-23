#ifndef STARBOARD_COMMON_SCOPED_TIMER_H_
#define STARBOARD_COMMON_SCOPED_TIMER_H_

#include <string_view>

#include "starboard/common/source_location.h"

namespace starboard {

// A ScopedTimer logs the time elapsed from its creation to its destruction.
// It can also be used to explicitly stop the timer and get the elapsed time.
class ScopedTimer {
 public:
  ScopedTimer(std::string_view message,
              SourceLocation location = SourceLocation::current());
  ~ScopedTimer();

  // Stops the timer, logs the elapsed time, and returns the duration.
  int64_t Stop();

 private:
  const std::string message_;
  const SourceLocation location_;
  const int64_t start_time_us_;
  bool stopped_ = false;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_SCOPED_TIMER_H_
