It is strongly encouraged for platforms to include the
`CobaltExtensionSocketReceiveMultiMsgApi` extension.

Starting in Cobalt 25.tls.20, a significant networking performance
improvement can be achieved on platforms that implement the
extension.

The extension allows a platform to provide an API to read multiple
UDP packets in a single call.  When on a Linux kernel, the `recvmmsg`
system call can be used for that resulting in a significantly reduced
amount of CPU usage and corresponding increase in networking
throughput.  This not only makes the application more responsive, but
also improves the video quality the devices can play.

A functioning implementation for Linux kernel based devices is
included in the 25.lts.20 source and can be added to a platform with
the changes below.  The changes are present in the source for the
Android and Linux targets.

To add the extension to a platform, (1) add the four source files
with the extension's implementation to the platform and (2) expand
`SbSystemGetExtension` to return the extension:

1.  In the platform's `BUILD.gn`: 
    Add the extension's implementation to the
    `static_library("starboard_platform")` section.

```
diff --git a/starboard/linux/shared/BUILD.gn b/starboard/linux/shared/BUILD.gn
index 6c01aecfe25..ddb3a4f666e 100644
--- a/starboard/linux/shared/BUILD.gn
+++ b/starboard/linux/shared/BUILD.gn
@@ -203,6 +203,10 @@ static_library("starboard_platform_sources") {
     "//starboard/shared/posix/socket_join_multicast_group.cc",
     "//starboard/shared/posix/socket_listen.cc",
     "//starboard/shared/posix/socket_receive_from.cc",
+    "//starboard/shared/posix/socket_receive_multi_msg_internal.cc",
+    "//starboard/shared/posix/socket_receive_multi_msg_internal.h",
+    "//starboard/shared/posix/socket_receive_multi_msg.cc",
+    "//starboard/shared/posix/socket_receive_multi_msg.h",
     "//starboard/shared/posix/socket_resolve.cc",
     "//starboard/shared/posix/socket_send_to.cc",
     "//starboard/shared/posix/socket_set_broadcast.cc",
```

2.  In the platform's `system_get_extensions.cc`: 
    Add two includes and a three line if statement.

```
diff --git a/starboard/linux/shared/system_get_extensions.cc b/starboard/linux/shared/system_get_extensions.cc
index 454bf3abe3f..59b45bc783e 100644
--- a/starboard/linux/shared/system_get_extensions.cc
+++ b/starboard/linux/shared/system_get_extensions.cc
@@ -29,6 +29,7 @@
 #endif
 #include "starboard/extension/memory_mapped_file.h"
 #include "starboard/extension/platform_service.h"
+#include "starboard/extension/socket_receive_multi_msg.h"
 #include "starboard/extension/time_zone.h"
 #include "starboard/linux/shared/configuration.h"
 #include "starboard/linux/shared/ifa.h"
@@ -38,6 +39,7 @@
 #include "starboard/shared/ffmpeg/ffmpeg_demuxer.h"
 #include "starboard/shared/posix/free_space.h"
 #include "starboard/shared/posix/memory_mapped_file.h"
+#include "starboard/shared/posix/socket_receive_multi_msg.h"
 #include "starboard/shared/starboard/application.h"
 #include "starboard/shared/starboard/crash_handler.h"
 #if SB_IS(EVERGREEN_COMPATIBLE)
@@ -71,6 +73,9 @@ const void* SbSystemGetExtension(const char* name) {
   if (strcmp(name, kCobaltExtensionFreeSpaceName) == 0) {
     return starboard::shared::posix::GetFreeSpaceApi();
   }
+  if (strcmp(name, kCobaltExtensionSocketReceiveMultiMsgName) == 0) {
+    return starboard::shared::posix::GetSocketReceiveMultiMsgApi();
+  }
 #if SB_API_VERSION < 15
   if (strcmp(name, kCobaltExtensionEnhancedAudioName) == 0) {
     return starboard::shared::enhanced_audio::GetEnhancedAudioApi();
```

Links:

+   The extension was added in PR [#4321](https://github.com/youtube/cobalt/pull/4321).
+   Linux man page for [recvmmsg](https://man7.org/linux/man-pages/man2/recvmmsg.2.html).
