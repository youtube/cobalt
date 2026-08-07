# SPEC-0002: Configurable Max Preraster Distance

Shortlink: TBD

## Overview

Chromium's compositor (`cc`) pre-rasterizes tiles outside the viewport to ensure smooth scrolling. The maximum distance for this pre-rasterization is controlled by `max_preraster_distance_in_screen_pixels` in `cc::LayerTreeSettings`, which defaults to 1000 pixels.

For TV devices with discrete scrolling, pre-rasterizing 1000 pixels ahead is often unnecessary and uses valuable GPU memory. This specification defines the requirements for making this distance configurable at runtime, allowing it to be reduced or disabled (set to 0) to save memory.

## Requirements

### REQ-0001: Feature/Flag Control
The maximum pre-raster distance SHALL be configurable at runtime.

*   **Scenario: Default Behavior**
    *   **GIVEN** No override is specified.
    *   **WHEN** Compositor settings are initialized.
    *   **THEN** `max_preraster_distance_in_screen_pixels` SHALL be set to the default value of 1000.

*   **Scenario: Overridden Behavior**
    *   **GIVEN** An override of 200 pixels is specified via command line or feature.
    *   **WHEN** Compositor settings are initialized.
    *   **THEN** `max_preraster_distance_in_screen_pixels` SHALL be set to 200.

*   **Scenario: Disabled Pre-rasterization**
    *   **GIVEN** An override of 0 pixels is specified.
    *   **WHEN** Compositor settings are initialized.
    *   **THEN** `max_preraster_distance_in_screen_pixels` SHALL be set to 0.

### REQ-0002: Validation
The configured pre-raster distance SHALL be non-negative. If a negative value is provided, it SHALL fall back to the default of 1000.

*   **Scenario: Negative Value**
    *   **GIVEN** An override of -500 pixels is specified.
    *   **WHEN** Compositor settings are initialized.
    *   **THEN** `max_preraster_distance_in_screen_pixels` SHALL fall back to 1000.
