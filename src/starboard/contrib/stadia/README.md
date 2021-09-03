### Reference implementation for Stadia

 This directory contains patches for Starboard required in order to use the Stadia web app from Cobalt.

## ./

  Files in the top-level directory offer the ability to pass binary messages back and forth between the Stadia web app and native Stadia services.

## ./clients
  Files in the clients/ subdirectory mimics the public interface of an underlying services library.

## ./x11
  This directory contains a fork of the starboard/shared/x11 code that integrates the Stadia-required changes in order to enable communication with an underlying shared library.
