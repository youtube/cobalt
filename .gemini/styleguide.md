# Cobalt projet code review style guide

All code submitted to this repository must adhere to the established upstream Chromium style guides.

Primary Reference: When reviewing code, the ultimate source of truth for style, naming conventions, formatting, and best practices resides under the root styleguide/ directory of the repository. Code reviews are expected to observe the relevant style guide for the language in question (e.g., C++, Java).

Note: In the context of these documents, any mention of "Chrome" should be interpreted as "Cobalt".

# Review Scope Exclusion (Backports and Cherry-Picks)

To prevent redundant reviews and streamline the process for necessary maintenance tasks, the following rule must be observed:

Skip Backports/Cherry-Picks: Do not perform a detailed code style review on pull requests (CLs) that are clearly marked as backports or cherry-picks from an already reviewed upstream change.

Rationale: These changes have already received mandatory review in the original source repository. A review of these CLs should focus only on ensuring the successful and correct application of the patch to the target branch.

Identification: Look for common keywords in the CL title, branch name, or description (e.g., backport, cherry-pick, (cr/123456), or reference to the upstream commit hash).

# Mojo Changes

To ensure high-quality Mojo changes, refer to these resources for best practices and common patterns:

*   **Mojo Style Guide**: /docs/mojo_and_services.md#Mojo-Style-Guide
*   **Mojo Common Patterns**: /docs/mojo_best_practices.md
*   **Mojo Testing**: /docs/mojo_testing.md
*   **Mojo Security**: /docs/security/mojo.md

# JNI Best Practices

For JNI (Java Native Interface) code, please adhere to the following best practices.

*   **JNI Best Practices**: /base/android/jni_generator/README.md#Best-Practices
*   **JNI Zero**: /third_party/jni_zero/README.md
*   **Android JNI Ownership Best Practices**: /docs/android_jni_ownership_best_practices.md
*   **Android Accessing Cpp Features in Java**: /docs/android_accessing_cpp_features_in_java.md
*   **Android Isolated Splits**: /docs/android_isolated_splits.md
*   **Binary Size Optimization Advice**: /docs/speed/binary_size/optimization_advice.md

# Web IDL Best Practices

For changes involving Web IDL, refer to the following guides for API design principles and Chromium-specific implementation details:

*   **Blink Web IDL Style Guide**: /docs/website/site/blink/webidl/index.md
*   **Chromium IDL Extended Attributes**: /third_party/blink/renderer/bindings/IDLExtendedAttributes.md
*   **Web IDL Interfaces**: /docs/website/site/developers/web-idl-interfaces/index.md

