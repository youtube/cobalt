# Cobalt Platform Services

_NOTE: The Cobalt Platform Services API replaces the deprecated Cobalt Custom Web API Extensions_

## Overview

The Cobalt Platform Services API aims to provide Cobalt users the ability to
extend the web application functionality of Cobalt. This is done using runtime
extensions provided by the Starboard layer without having to make any
modifications to the common Cobalt code.

Web applications running on Cobalt may want to implement a feature to have
direct access to platform-specific services, on platforms where they are
available.

For example, a Cobalt implementation may want to enable the web application to:

*   Communicate directly with a service that is available on the platform
*   Surface platform-level status messages

The Cobalt Platform Services API is an RPC interface and makes assumptions that
communication with the service is in-process and 100% reliable, unlike sockets
or HTTP. Out-of-process communications can be implemented as a layer on top of
the interface.

## Interface Definition

The Cobalt Platform Services API is an RPC interface allowing for bidirectional
communication between the web application and platform service. The interface
is intentionally minimal, to avoid as much as possible the need to make changes
to common Cobalt code. There will be two parallel interfaces, one between the
web app and Cobalt specified via IDL, and another between Cobalt and the
Starboard implementation specified via a Starboard interface header file.

The interface provides a method of querying for services by name, where a name
is some arbitrary string (Java naming convention recommended). The platform
should maintain a registry mapping names to services and return the appropriate
service for the given name. Once the application has obtained a handle to a
service, it can send messages to the service using a `send()` function, which
optionally may return immediate results. In addition, it can receive
asynchronous incoming messages from the service via a receive callback
registered when the service is opened. The `send()` function may fail, either
because the service has already been closed by the application, or because of
some state of the service and platform. When `send()` fails, it will raise an
`kInvalidStateErr` exception, which may be caught & handled by the application.

Any functionality in addition to the necessities provided by the interface must
be implemented within the service’s protocol.  For example, services may push a
“version” message to the application immediately upon being opened in order to
implement versioning.

### IDL Interface

The Platform Services extension is exposed to the web app via the following IDL:

*   [src/cobalt/h5vcc/h5vcc\_platform\_service.idl](../h5vcc/h5vcc_platform_service.idl)

The entrypoint for defined Platform Services extensions will be accessible in
the `H5vccPlatformService` object. Note that ArrayBuffers are chosen to
represent arbitrary message data in order to leave open the possibility of
passing binary data.

### Starboard Interface

Implementing the Starboard layer of Platform Service extension support uses the
following interface in parallel with the IDL interface:

*   [src/cobalt/extension/platform\_service.h](../extension/platform_service.h)

`CobaltExtensionPlatformServiceApi` is the main interface for the Starboard
layer.

### Naming Conventions for Service Names

In order to guard against namespace pollution, the Java package naming
convention is used for Platform Services.

For example, some services names could be:

*   `com.google.android.CobaltPlatformService`
*   `com.<PARTNER>.<PLATFORM>.CobaltPlatformService`

## Using a Cobalt Platforms Service Extension

Assuming that the implementation work is completed for the IDL and
complimentary Starboard interface, an example usage of an extension from the
web application could look like the following:

```
var myCobaltPlatformService = "com.google.android.CobaltPlatformService";

// checks if a platform has the specified service available
H5vccPlatformService.has(myCobaltPlatformService);

// attempting to open the specified service
service = H5vccPlatformService.open(myCobaltPlatformService,
                            (service, data) => { console.log("do something") };

// send some data
var data = new ArrayBuffer(16);
var view = new DataView(data);
view.setInt32(0, 0x01234567);
service.send(data);

// close the service when we are done
service.close();
```
