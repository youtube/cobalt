
#include <jni.h>

#include "jni_vars.h"

namespace {
JavaVM* g_vm = NULL;
jobject g_application_class_loader = NULL;
jobject g_starboard_bridge = NULL;
}  // namespace

namespace starboard {
namespace android {
namespace shared {

__attribute__((visibility("default"))) void JNIState::SetVM(JavaVM* vm) {
  g_vm = vm;
}

__attribute__((visibility("default"))) JavaVM*& JNIState::GetVM() {
  return g_vm;
}
__attribute__((visibility("default"))) void JNIState::SetBridge(jobject value) {
  g_starboard_bridge = value;
}
jobject& JNIState::GetBridge() {
  return g_starboard_bridge;
}
void JNIState::SetApplicationClassLoader(jobject value) {
  g_application_class_loader = value;
}
jobject& JNIState::GetApplicationClassLoader() {
  return g_application_class_loader;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
