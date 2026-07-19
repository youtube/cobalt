# SPEC-0001: Configurable Display Lock Viewport Margin

Shortlink: TBD

## Overview

 BBox (Display Lock) in Blink uses a hardcoded margin of 150% of the viewport to determine when an element with `content-visibility: auto` is considered "on-screen" (intersecting the viewport). If it is "off-screen" (outside viewport + margin), its descendants are not laid out or painted.

For Cobalt (TV devices), this 150% margin is too generous and wastes memory. This specification defines the requirements for making this margin configurable at runtime via feature flags, enabling aggressive pruning of off-screen elements.

## Requirements

### REQ-0001: Feature Flag Control
The viewport margin percentage used by Display Lock SHALL be configurable via a Blink Feature Parameter.

*   **Scenario: Default Behavior**
    *   **GIVEN** The configuration feature is disabled.
    *   **WHEN** Display Lock initializes its internal `IntersectionObserver`.
    *   **THEN** The margin SHALL be set to the default value of 150%.

*   **Scenario: Overridden Behavior**
    *   **GIVEN** The configuration feature is enabled with a parameter value of `20.0`.
    *   **WHEN** Display Lock initializes its internal `IntersectionObserver`.
    *   **THEN** The margin SHALL be set to 20% of the viewport.

*   **Scenario: Zero Margin (Aggressive Pruning)**
    *   **GIVEN** The configuration feature is enabled with a parameter value of `0.0`.
    *   **WHEN** Display Lock initializes its internal `IntersectionObserver`.
    *   **THEN** The margin SHALL be set to 0% (exact viewport boundary).

### REQ-0002: Validation
The configured margin percentage value SHALL be non-negative. If a negative value is provided, it SHALL fall back to the default of 150%.

*   **Scenario: Negative Value**
    *   **GIVEN** The configuration feature is enabled with a parameter value of `-50.0`.
    *   **WHEN** Display Lock initializes its internal `IntersectionObserver`.
    *   **THEN** The margin SHALL fall back to the default of 150%.
