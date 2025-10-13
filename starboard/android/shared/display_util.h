#ifndef STARBOARD_ANDROID_SHARED_DISPLAY_UTIL_H_
#define STARBOARD_ANDROID_SHARED_DISPLAY_UTIL_H_

namespace starboard {

// A gateway to access DisplayUtil.java through JNI.
class DisplayUtil {
 public:
  struct Dpi {
    float x;
    float y;
  };

  // Returns the physical pixels per inch of the screen in the X and Y
  // dimensions.
  static Dpi GetDisplayDpi();
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_DISPLAY_UTIL_H_
