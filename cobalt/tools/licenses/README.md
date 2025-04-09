# licenses.py

Utility for checking and processing licensing information in
third_party directories.

## Usage

```shell
cobalt/tools/licenses.py *command* [OPTIONAL ARGS]
```

### Command Choices

    - help
    - scan
    - credits
    - license_file

### Example

```
tools/licenses/licenses.py credits --gn-target cobalt:gn_all --gn-out-dir out/android-arm_gold > ~/license-file
```

## Optional Args

    - file-template - Template HTML to use for the license page.
    - entry-template - Template HTML to use for each license.
    - extra-third-party-dirs - Gn list of additional third_party dirs to look through.
    - extra-allowed-dirs - List of extra allowed paths to look for deps that will be picked up automatically besides third_party.
    - target-os - OS that this build is targeting.
    - gn-out-dir - GN output directory for scanning dependencies.
    - gn-target - GN target to scan for dependencies.
    - format - What format to output in.
        - default - 'txt'
        - choices - 'txt', 'spdx'
    - spdx-root - Use a different root for license refs than _REPOSITORY_ROOT.
        - default - _REPOSITORY_ROOT
    - spdx-link - Link prefix for license cross ref.
        - default - 'https://source.chromium.org/chromium/chromium/src/+/main:'
    - spdx-doc-name - Specify a document name for the SPDX doc.
    - spdx-doc-namespace - Specify the document namespace for the SPDX doc.
        - default - 'https://chromium.googlesource.com/chromium/src/tools/'
