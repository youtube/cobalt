# Cobalt Mystery HUD Menu (6-Button Remote Compositor HUD Toggler)

## Overview

The **Mystery HUD Menu** provides an on-screen interactive menu for toggling Chromium compositor Heads-Up Display (HUD) overlays using only the **6 standard TV remote buttons** (`Up`, `Down`, `Left`, `Right`, `OK`/`Select`, and `Back`).

This feature is compiled into all non-gold Cobalt builds (`debug`, `devel`, and `qa`) via:
```cpp
#if BUILDFLAG(IS_COBALT) && !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
```
It is completely excluded from `gold` production release builds.

---

## 1. Unlocking the Mystery HUD Menu

To open the Mystery HUD Menu from any screen in the application, enter the following **10-key sequence** on the TV remote or keyboard:

$$\texttt{Up} \rightarrow \texttt{Up} \rightarrow \texttt{Down} \rightarrow \texttt{Down} \rightarrow \texttt{Left} \rightarrow \texttt{Right} \rightarrow \texttt{Left} \rightarrow \texttt{Right} \rightarrow \textbf{Back} \rightarrow \textbf{OK}$$

### Accepted Key Codes at Each Step

| Sequence Step | Button | Accepted Virtual Key Codes (`ui::KeyboardCode`) |
| :--- | :--- | :--- |
| **Steps 1–2** | `Up` (`↑`) | `VKEY_UP` |
| **Steps 3–4** | `Down` (`↓`) | `VKEY_DOWN` |
| **Step 5** | `Left` (`←`) | `VKEY_LEFT` |
| **Step 6** | `Right` (`→`) | `VKEY_RIGHT` |
| **Step 7** | `Left` (`←`) | `VKEY_LEFT` |
| **Step 8** | `Right` (`→`) | `VKEY_RIGHT` |
| **Step 9** | `Back` (or `OK` fallback) | `VKEY_ESCAPE`, `VKEY_BACK`, `VKEY_BROWSER_BACK`, `VKEY_B`, or **`OK` (`VKEY_RETURN`, `VKEY_SELECT`, `VKEY_ACCEPT`, `VKEY_A`)** |
| **Step 10** | `OK` / `Select` | `VKEY_RETURN`, `VKEY_SELECT`, `VKEY_ACCEPT`, `VKEY_A` |

> **Note on Platform Shell `Back` Interception**: Some TV OS shells or remote injection bridges intercept the `Back` button before it reaches Cobalt. To ensure the sequence works across all devices and remote injection tools, **Step 9 also accepts `OK`** (`↑ ↑ ↓ ↓ ← → ← → OK OK`).

### Input Event Consumption
- **Steps 1–8 (`↑ ↑ ↓ ↓ ← → ← →`)**: Pass through normally to the web application (standard D-pad focus movement).
- **Step 9 (`Back`) and Step 10 (`OK`)**: Consumed by `WebFrameWidgetImpl::HandleKeyEvent` (`WebInputEventResult::kHandledSystem`) so the web application does not navigate back/exit or activate the currently focused tile when completing the sequence.

---

## 2. Interactive 10-Second HUD Control Window

Once the 10-key sequence is entered:
1. The **Mystery HUD Menu** activates (`LayerTreeDebugState::mystery_hud_menu_active = true`) in the **top-right** corner of the HUD overlay (independent of the FPS & GPU Memory readout in the top-left corner).
2. If all HUD overlays were previously `OFF`, **FPS & GPU Memory (`show_fps_counter`)** is automatically turned **`ON`** to provide immediate visual feedback.
3. A **10-second inactivity timer** starts. Every button press while the menu is open resets the 10-second timer.
4. While the menu is open, all keys are intercepted (`kHandledSystem`) so D-pad inputs toggle compositor overlays without moving focus in the underlying web app.

### Menu Controls

| Remote Button | Action | `cc::LayerTreeDebugState` Field | Description |
| :--- | :--- | :--- | :--- |
| **`Up` (`↑`)** | Toggle **FPS & Memory** | `show_fps_counter` | Displays real-time frame rate (FPS), frame timing histogram, GPU raster status, and compositor `ResourcePool` GPU memory usage (`used` / `max` MB). |
| **`Right` (`→`)** | Toggle **Borders** | `show_debug_borders` | Draws color-coded borders around composited layers, render surfaces, and compositor tiles. |
| **`Down` (`↓`)** | Toggle **Paint Flash** | `show_paint_rects` | Highlights regions of the screen that are being repainted on each frame. |
| **`Left` (`←`)** | Toggle **Layout Shift** | `show_layout_shift_regions` | Highlights regions experiencing Cumulative Layout Shift (CLS). |
| **`OK` (`Select`)** | **Close & Keep** | `mystery_hud_menu_active = false` | Closes the Mystery HUD Menu immediately while **keeping** all currently enabled HUD overlays active. |
| **`Back`** | **Reset All & Close** | All HUD flags `= false` | Resets all HUD overlays (`show_fps_counter`, `show_debug_borders`, `show_paint_rects`, `show_layout_shift_regions`) to **`OFF`** and closes the menu. |

### Auto-Close after 10 Seconds
If no button is pressed for **10 seconds**, `WebFrameWidgetImpl::OnMysteryMenuTimeout` automatically closes the Mystery HUD Menu (`mystery_hud_menu_active = false`) while **preserving** whichever overlays are currently toggled `ON`, restoring normal remote control to the web application.

---

## 3. On-Screen HUD Appearance

When active, `HeadsUpDisplayLayerImpl::DrawMysteryHudMenu` renders the following status box in the **top-right corner** of the screen (while the FPS and GPU Memory readout renders in the **top-left corner**):

```text
Mystery HUD Menu
[UP] FPS & Memory       ON
[RIGHT] Borders        OFF
[DOWN] Paint Flash     OFF
[LEFT] Layout Shift    OFF
[OK] Close  [BACK] Reset
```

