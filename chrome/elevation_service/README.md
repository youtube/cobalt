This directory contains the code for a COM server that handles elevation
requests. This code is compiled into an executable named elevation_service.exe.

This is a standalone executable.

Currently, the elevation service is only installed for Google Chrome builds. The
primary use case at the moment for the service has to do with the Chrome
recovery component. The recovery component is registered only for Google Chrome
builds. It repairs the Chrome updater (Google Update) when the algorithm detects
that Chrome is not being updated. Since Chrome could be installed per-system or
per-user, an elevation service is needed to repair the code in the per-system
install case.

