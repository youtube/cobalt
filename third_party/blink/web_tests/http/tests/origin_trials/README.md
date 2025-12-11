This directory contains tests to ensure that the origin trials framework
correctly handles enables/disables trials based on combinations of:
- Tokens missing or invalid
- Tokens provided via header, <meta> tag, and injected via script
- Secure and insecure contexts
- JS exposure via bindings for various IDL constructs

The conventions for test file naming:
- If the name contains "enabled", it generally means that a valid token is
  provided, regardless if the trial actually ends up being enabled.
- If the name contains "disabled", it generally means that no token is provided.
TODO(chasej): Rename the test files for clarity, so we don't need to explain
  the naming convention. e.g. "has-token", "valid-token" vs "no-token". Then
  can use "enabled" or "disabled" to reflect expected status.
