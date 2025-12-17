#ifndef STARBOARD_LOADER_APP_STARTUP_TIMER_H_
#define STARBOARD_LOADER_APP_STARTUP_TIMER_H_

#include "starboard/common/time.h"
#include "starboard/export.h"

namespace loader_app {
namespace StartupTimer {

// Returns the time when the app started.
int64_t StartTime();

// Returns the time elapsed since the app started.
int64_t TimeElapsed();

// Sets the application startup time.
void SetAppStartupTime();

// Returns the application startup time.
SB_EXPORT int64_t GetAppStartupTime();

}  // namespace StartupTimer
}  // namespace loader_app

#endif  // STARBOARD_LOADER_APP_STARTUP_TIMER_H_
