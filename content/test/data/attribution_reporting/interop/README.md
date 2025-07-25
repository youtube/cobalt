# Interop Tests

This directory contains a set of tests which ensure the attribution logic as
implemented matches the intended behavior of the Attribution Reporting API.

See https://wicg.github.io/attribution-reporting-api/ for the draft specification.

See //content/browser/attribution_reporting/attribution_interop_unittest.cc
for the tests.

These tests are purposefully not implemented as web platform tests, so that
they can be shared by non-web-based platforms.

The tests here cover how the browser will handle various series of sources and
triggers with different configurations, but does not rely on any blink APIs.

The vendor-specific parameters can be configured for testing. The default
configuration is specified in `default_config.json` that is contained in this
directory. Each test file can optionally specify the parameters in the
"api_config" field.

# Configuration format

The JSON schema is as follows. All the fields are required in "default_config.json"
and optional in "api_config" field. See the schema in "default_config.json".

# Test case format

The JSON schema is as follows. Timestamps must be distinct across all sources
and triggers.

```jsonc
{
  "description": "description",

  // Optional configuration.
  "api_config": {},

  // Required input.
  "input": {
    // List of zero or more registrations.
    "registrations": [
      {
        // Required time at which to register in milliseconds since
        // the UNIX epoch formatted as a base-10 string.
        "timestamp": "123",

        "registration_request": {
          // Required URL specified in the attributionsrc registration.
          "attribution_src_url": "https://reporting.example",

          // Required origin on which to register.
          "context_origin": "https://context.example",

          // Either "navigation" or "event",
          // corresponding to whether the source is registered on click or
          // view, respectively. Must be present for source registrations and
          // omitted for trigger registrations.
          "source_type": "navigation"
        },

        // List of URLs and the corresponding responses. Currently only allows
        // one.
        "responses": [
          {
            // Required URL from which the response was sent.
            "url": "https://reporting.example",

            // Whether the registration will be processed with debug permission
            // enabled. Defaults to false.
            "debug_permission": true,

            // Exactly one of the below fields must be present. See
            // https://github.com/WICG/attribution-reporting-api for the
            // complete schema.
            "response": {
              "Attribution-Reporting-Register-Source": { ... },

              "Attribution-Reporting-Register-Trigger": { ... }
            }
          }
        ]
      }
    ]
  },

  // Expected output.
  "output": {
    "reports": [
      {
        // URL to which the report would have been sent.
        "report_url": "https://reporting.example/.well-known/attribution-reporting/report-event-attribution",

        // Time at which the report would have been sent in milliseconds since
        // the UNIX epoch.
        "report_time": "123",

        "payload": ...
      }
    ],

    "unparsable_registrations": [
      {
        // Time of the input that failed to register.
        "time": "123",

        "type": "source" // or "trigger"
      }
    ]
  }
}
```
