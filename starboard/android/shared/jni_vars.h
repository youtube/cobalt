

#include <jni.h>

namespace starboard {
namespace android {
namespace shared {

class JNIState {
 public:
  static __attribute__((visibility("default"))) void SetVM(JavaVM* vm);
  static __attribute__((visibility("default"))) JavaVM*& GetVM();
  static __attribute__((visibility("default"))) void SetBridge(jobject value);
  static __attribute__((visibility("default"))) jobject& GetBridge();
  static __attribute__((visibility("default"))) void SetApplicationClassLoader(
      jobject value);
  static __attribute__((visibility("default"))) jobject&
  GetApplicationClassLoader();
};

}  // namespace shared
}  // namespace android
}  // namespace starboard
