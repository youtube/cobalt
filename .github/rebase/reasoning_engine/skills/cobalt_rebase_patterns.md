# Cobalt Rebase Patterns

### Handling Cobalt-Specific Version Macros

A common pattern in the Cobalt codebase is the use of preprocessor macros to conditionally compile code based on the Chromium milestone version. These macros follow the format `CHROMIUM_MILESTONE_LE_XXX`, where `XXX` is a Chromium version number (e.g., `CHROMIUM_MILESTONE_LE_138`).

When performing a rebase to a new Chromium version `YYY`, you must:
1.  Globally search the codebase for the pattern `CHROMIUM_MILESTONE_LE_`.
2.  For any macros referencing the *previous* version, update them to the *new* version number. For example, when rebasing from 138 to 140, all instances of `CHROMIUM_MILESTONE_LE_138` should be updated to `CHROMIUM_MILESTONE_LE_140` (or a higher version like 150 if the feature lifetime has been extended, as seen in PR #12161).
3.  This is a critical step for managing API churn and feature flags between versions. Failure to update these macros will result in using stale code paths.
