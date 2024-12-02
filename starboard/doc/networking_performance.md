# Networking performance

## Overview

Cobalt 25 implements IETF HTTP/3 which uses the QUIC transport protocol which is implemented using the UDP protocol and application level stream multiplexing, flow control, congestion avoidance, connection establishment, field compression, etc.

For HTTP/3, Cobalt benefits from more specific tuning and customizations to improve its performance.

## Starboard Extension

It is strongly encouraged for platforms to include the
`CobaltExtensionSocketReceiveMultiMsgApi` extension.

Starting in Cobalt 25.tls.20, a significant networking performance
improvement can be achieved on platforms that implement the
extension.

The extension allows a platform to provide an API to read multiple
UDP packets in a single call.  When on a Linux kernel, the `recvmmsg`
system call can be used and results in a significantly reduced amount
of CPU usage and corresponding increase in networking throughput.
This not only makes the application more responsive, but also
improves the video quality that the devices can play.

## Implementation details

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
    patch -p1 <<EOF
    --- a/starboard/linux/shared/BUILD.gn
    +++ b/starboard/linux/shared/BUILD.gn
    @@ -205,2 +205,6 @@ static_library("starboard_platform_sources") {
         "//starboard/shared/posix/socket_receive_from.cc",
    +    "//starboard/shared/posix/socket_receive_multi_msg_internal.cc",
    +    "//starboard/shared/posix/socket_receive_multi_msg_internal.h",
    +    "//starboard/shared/posix/socket_receive_multi_msg.cc",
    +    "//starboard/shared/posix/socket_receive_multi_msg.h",
         "//starboard/shared/posix/socket_resolve.cc",
    EOF
    ```

2.  In the platform's `system_get_extensions.cc`:
    Add two includes and a three line if statement.

    ```
    patch -p1 <<EOF
    --- a/starboard/linux/shared/system_get_extensions.cc
    +++ b/starboard/linux/shared/system_get_extensions.cc
    @@ -31,2 +31,3 @@
     #include "starboard/extension/platform_service.h"
    +#include "starboard/extension/socket_receive_multi_msg.h"
     #include "starboard/extension/time_zone.h"
    @@ -40,2 +41,3 @@
     #include "starboard/shared/posix/memory_mapped_file.h"
    +#include "starboard/shared/posix/socket_receive_multi_msg.h"
     #include "starboard/shared/starboard/application.h"
    @@ -71,3 +73,6 @@ const void* SbSystemGetExtension(const char* name) {
       if (strcmp(name, kCobaltExtensionFreeSpaceName) == 0) {
         return starboard::shared::posix::GetFreeSpaceApi();
       }
    +  if (strcmp(name, kCobaltExtensionSocketReceiveMultiMsgName) == 0) {
    +    return starboard::shared::posix::GetSocketReceiveMultiMsgApi();
    +  }
    EOF
    ```

## Links

+   The extension was added in PR [#4321](https://github.com/youtube/cobalt/pull/4321).
+   Linux man page for [recvmmsg](https://man7.org/linux/man-pages/man2/recvmmsg.2.html).
+   HTTP/3 explainer <https://en.wikipedia.org/wiki/HTTP/3>
+   [RFC9114] Bishop, M., Ed., "HTTP/3", RFC 9114, DOI 10.17487/RFC9114, June 2022, <https://www.rfc-editor.org/info/rfc9114>.
+   [RFC9000] Iyengar, J., Ed., and M. Thomson, Ed., "QUIC: A UDP-Based Multiplexed and Secure Transport", RFC 9000, DOI 10.17487/RFC9000, May 2021, <https://www.rfc-editor.org/info/rfc9000>.
