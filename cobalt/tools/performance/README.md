# Cobalt Performance Test Tooling

This enables automated testing of Cobalt/Chrobalt & Kimono using some tooling in
//third_party/catapult/telemetry & //third_party/catapult/devil.

## Usage

```
devil_script.py [-c/--is_cobalt <True/T/t/False/F/f>]
```

Note: This defaults to launching tests on Kimono, but offers a switch to use Cobalt instead.
      This is especially useful if you only need to test functionality in Cobalt and not Kimono.
