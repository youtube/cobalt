# External Mojo services

This directory contains code that allows Mojo services in processes outside of
cast_shell to communicate with Mojo services within cast_shell.

## broker_service
This contains the service_manager::Service that runs inside cast_shell and
brokers communication between Mojo services in external processes and services
within cast_shell. This service is automatically started when the
'enable_external_mojo_services' GN arg is true.

## external_service_support
This directory contains utilities to ease development of Mojo services that run
in processes outside of Chromium/cast_shell.

## common
This contains code that is shared between cast_shell and external services.
