// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomCallbackMacro} from '/common/action_fulfillment/macros/custom_callback_macro.js';
import {KeyCombination, KeyPressMacro} from '/common/action_fulfillment/macros/key_press_macro.js';
import {Macro, ToggleDirection} from '/common/action_fulfillment/macros/macro.js';
import {MacroName} from '/common/action_fulfillment/macros/macro_names.js';
import {MouseClickLeftDoubleMacro, MouseClickLeftTripleMacro, MouseClickMacro} from '/common/action_fulfillment/macros/mouse_click_macro.js';
import {ToggleDictationMacro} from '/common/action_fulfillment/macros/toggle_dictation_macro.js';
import {AsyncUtil} from '/common/async_util.js';
import {KeyCode} from '/common/key_code.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';
import type {FaceLandmarkerResult} from '/third_party/mediapipe/vision.js';

import {BubbleController} from './bubble_controller.js';
import {FacialGesture} from './facial_gestures.js';
import {GestureDetector} from './gesture_detector.js';
import {MouseLongClickMacro} from './macros/mouse_long_click_macro.js';
import {MouseScrollMacro} from './macros/mouse_scroll_macro.js';
import {ResetCursorMacro} from './macros/reset_cursor_macro.js';
import {MouseController} from './mouse_controller.js';

import RoleType = chrome.automation.RoleType;
import StateType = chrome.automation.StateType;

type AutomationNode = chrome.automation.AutomationNode;
type PrefObject = chrome.settingsPrivate.PrefObject;

interface DetectMacrosResult {
  macros: Macro[];
  displayText: string;
}

/** Handles converting facial gestures to Macros. */
export class GestureHandler {
  private gesturesToKeyCombos_: Map<FacialGesture, KeyCombination> = new Map();
  private gestureToMacroName_: Map<FacialGesture, MacroName> = new Map();
  private gestureToConfidence_: Map<FacialGesture, number> = new Map();
  private gestureLastRecognized_: Map<FacialGesture, number> = new Map();
  private mouseController_: MouseController;
  private bubbleController_: BubbleController;
  private repeatDelayMs_ = GestureHandler.DEFAULT_REPEAT_DELAY_MS;
  private prefsListener_: (prefs: any) => void;
  private toggleInfoListener_: (enabled: boolean) => void;
  // The most recently detected gestures. We track this to know when a gesture
  // has ended.
  private previousGestures_: FacialGesture[] = [];
  private macrosToCompleteLater_:
      Map<FacialGesture, {macro: Macro, displayText: string}> = new Map();
  private paused_ = false;
  private isDictationActive_: () => boolean;

  constructor(
      mouseController: MouseController, bubbleController: BubbleController,
      isDictationActive: () => boolean) {
    this.mouseController_ = mouseController;
    this.bubbleController_ = bubbleController;
    this.isDictationActive_ = isDictationActive;
    this.prefsListener_ = prefs => this.updateFromPrefs_(prefs);
    this.toggleInfoListener_ = enabled =>
        GestureDetector.toggleSendGestureDetectionInfo(enabled);
  }

  start(): void {
    this.paused_ = false;
    chrome.settingsPrivate.getAllPrefs(prefs => this.updateFromPrefs_(prefs));
    chrome.settingsPrivate.onPrefsChanged.addListener(this.prefsListener_);

    chrome.accessibilityPrivate.onToggleGestureInfoForSettings.addListener(
        this.toggleInfoListener_);
  }

  stop(): void {
    this.paused_ = false;
    chrome.settingsPrivate.onPrefsChanged.removeListener(this.prefsListener_);
    chrome.accessibilityPrivate.onToggleGestureInfoForSettings.removeListener(
        this.toggleInfoListener_);
    this.previousGestures_ = [];
    this.gestureLastRecognized_.clear();
    // Executing these macros clears their state, so that we aren't left in a
    // mouse down or key down state.
    this.macrosToCompleteLater_.forEach((entry) => {
      entry.macro.run();
    });
    this.macrosToCompleteLater_.clear();
  }

  isPaused(): boolean {
    return this.paused_;
  }

  private updateFromPrefs_(prefs: PrefObject[]): void {
    prefs.forEach(pref => {
      switch (pref.key) {
        case GestureHandler.GESTURE_TO_MACRO_PREF:
          if (pref.value) {
            // Update the whole map from this preference.
            this.gestureToMacroName_.clear();

            let hasScrollModeAction = false;
            let hasLongClickAction = false;
            for (const [gesture, assignedMacro] of Object.entries(pref.value)) {
              if (assignedMacro === MacroName.UNSPECIFIED) {
                continue;
              }

              if (assignedMacro === MacroName.TOGGLE_SCROLL_MODE) {
                hasScrollModeAction = true;
              }

              if (assignedMacro === MacroName.MOUSE_LONG_CLICK_LEFT) {
                hasLongClickAction = true;
              }

              this.gestureToMacroName_.set(
                  gesture as FacialGesture, Number(assignedMacro));
              // Ensure the confidence for this gesture is set to the default,
              // if it wasn't set yet. This might happen if the user hasn't
              // opened the settings subpage yet.
              if (!this.gestureToConfidence_.has(gesture as FacialGesture)) {
                this.gestureToConfidence_.set(
                    gesture as FacialGesture,
                    GestureHandler.DEFAULT_CONFIDENCE_THRESHOLD);
              }
            }

            // If a "toggle" action is removed while the relevant action
            // is active, then we should toggle out of the action. Otherwise,
            // the user will be stuck in the action with no way to exit.
            if (this.mouseController_.isScrollModeActive() &&
                !hasScrollModeAction) {
              this.mouseController_.toggleScrollMode();
            }

            if (this.mouseController_.isLongClickActive() &&
                !hasLongClickAction) {
              this.mouseController_.toggleLongClick();
            }
          }
          break;
        case GestureHandler.GESTURE_TO_CONFIDENCE_PREF:
          if (pref.value) {
            for (const [gesture, confidence] of Object.entries(pref.value)) {
              this.gestureToConfidence_.set(
                  gesture as FacialGesture, Number(confidence) / 100.);
            }
          }
          break;
        case GestureHandler.GESTURE_TO_KEY_COMBO_PREF:
          if (pref.value) {
            // Update the whole map from this preference.
            this.gesturesToKeyCombos_.clear();
            for (const [gesture, keyCombinationAsString] of Object.entries(
                     pref.value)) {
              const keyCombination =
                  JSON.parse(keyCombinationAsString as string);
              this.gesturesToKeyCombos_.set(
                  gesture as FacialGesture, keyCombination);
            }
          }
          break;
        default:
          return;
      }
    });
  }

  getHeldMacroDisplayStrings(): string[] {
    const displayStrings: string[] = [];
    for (const entry of this.macrosToCompleteLater_.values()) {
      displayStrings.push(entry.displayText);
    }
    return displayStrings;
  }

  detectMacros(result: FaceLandmarkerResult): DetectMacrosResult {
    const gestures = GestureDetector.detect(result, this.gestureToConfidence_);
    const {macros, displayText} = this.gesturesToMacros_(gestures);
    const macrosOnGestureEnd =
        this.popMacrosOnGestureEnd(gestures, this.previousGestures_);
    // Because these macros are finished when the gesture is released rather
    // than when the gesture is triggered for the second time, the bubble needs
    // to be manually reset here to ensure the corresponding macro description
    // is cleared rather than waiting for another FaceLandmarkerResult.
    if (macrosOnGestureEnd.length > 0) {
      this.bubbleController_.resetBubble();
    }
    macros.push(...macrosOnGestureEnd);
    this.previousGestures_ = gestures;
    return {macros, displayText};
  }

  togglePaused(gesture: FacialGesture): void {
    const newPaused = !this.paused_;
    const lastToggledTime = this.gestureLastRecognized_.get(gesture);

    // Run start/stop before assigning the new pause value and gesture last
    // recognized time, since start/stop will modify these values.
    newPaused ? this.stop() : this.start();

    if (lastToggledTime) {
      this.gestureLastRecognized_.set(gesture, lastToggledTime);
    }

    this.paused_ = newPaused;
  }

  getGestureForPause(): FacialGesture|undefined {
    return this.getGestureFor_(MacroName.TOGGLE_FACEGAZE);
  }

  getGestureForScroll(): FacialGesture|undefined {
    return this.getGestureFor_(MacroName.TOGGLE_SCROLL_MODE);
  }

  getGestureForLongClick(): FacialGesture|undefined {
    return this.getGestureFor_(MacroName.MOUSE_LONG_CLICK_LEFT);
  }

  getGestureForDictation(): FacialGesture|undefined {
    return this.getGestureFor_(MacroName.TOGGLE_DICTATION);
  }

  private getGestureFor_(macroName: MacroName): FacialGesture|undefined {
    // Return the first found gesture assigned to the given macro.
    for (const [gesture, macro] of this.gestureToMacroName_.entries()) {
      if (macro === macroName) {
        return gesture;
      }
    }
    return undefined;
  }

  private gesturesToMacros_(gestures: FacialGesture[]): DetectMacrosResult {
    const macroNames: Map<MacroName, FacialGesture> = new Map();
    for (const gesture of gestures) {
      const currentTime = new Date().getTime();
      if (this.gestureLastRecognized_.has(gesture) &&
              currentTime - this.gestureLastRecognized_.get(gesture)! <
                  this.repeatDelayMs_ ||
          this.macrosToCompleteLater_.has(gesture)) {
        // Avoid responding to the same macro repeatedly in too short a time
        // or if we are still waiting to complete them later (they shouldn't be
        // repeated until completed).
        continue;
      }
      this.gestureLastRecognized_.set(gesture, currentTime);
      const name = this.gestureToMacroName_.get(gesture);
      if (name) {
        macroNames.set(name, gesture);
      }
    }

    // Construct display text.
    const displayStrings: string[] = [];
    // Construct macros from all the macro names.
    const result: Macro[] = [];
    for (const [macroName, gesture] of macroNames) {
      const macro = this.macroFromName_(macroName, gesture);
      if (macro) {
        result.push(macro);
        const displayText = BubbleController.getDisplayText(gesture, macro);
        displayStrings.push(displayText);
        if (macro.triggersAtActionStartAndEnd()) {
          // Cache this macro to be run a second time later,
          // e.g. for key release.
          this.macrosToCompleteLater_.set(
              gesture, {macro: macro, displayText: displayText});
        }
      }
    }

    const displayText = displayStrings.join(', ');
    return {macros: result, displayText};
  }

  /**
   * Gets the cached macros that are run again when a gesture ends. For example,
   * for a key press macro, the key press starts when the gesture is first
   * detected and the macro is run a second time when the gesture is no longer
   * detected, thus the key press will be held as long as the gesture is still
   * detected.
   */
  private popMacrosOnGestureEnd(
      gestures: FacialGesture[], previousGestures: FacialGesture[]): Macro[] {
    const macrosForLater: Macro[] = [];
    previousGestures.forEach(previousGesture => {
      if (!gestures.includes(previousGesture)) {
        // The gesture has stopped being recognized. Run the second half of this
        // macro, and stop saving it.
        const entry = this.macrosToCompleteLater_.get(previousGesture);
        if (!entry || !entry.macro) {
          return;
        }
        macrosForLater.push(entry.macro);
        this.macrosToCompleteLater_.delete(previousGesture);
      }
    });
    return macrosForLater;
  }

  private macroFromName_(name: MacroName, gesture: FacialGesture): Macro
      |undefined {
    if (!this.isMacroAllowed_(name)) {
      return;
    }

    switch (name) {
      case MacroName.TOGGLE_DICTATION:
        return new ToggleDictationMacro(
            /*dictationActive=*/ this.isDictationActive_());
      case MacroName.MOUSE_CLICK_LEFT:
        return new MouseClickMacro(this.mouseController_.mouseLocation());
      case MacroName.MOUSE_CLICK_RIGHT:
        return new MouseClickMacro(
            this.mouseController_.mouseLocation(), /*leftClick=*/ false);
      case MacroName.MOUSE_LONG_CLICK_LEFT:
        return new MouseLongClickMacro(this.mouseController_);
      case MacroName.MOUSE_CLICK_LEFT_DOUBLE:
        return new MouseClickLeftDoubleMacro(
            this.mouseController_.mouseLocation());
      case MacroName.MOUSE_CLICK_LEFT_TRIPLE:
        return new MouseClickLeftTripleMacro(
            this.mouseController_.mouseLocation());
      case MacroName.RESET_CURSOR:
        return new ResetCursorMacro(this.mouseController_);
      case MacroName.KEY_PRESS_SPACE:
        return new KeyPressMacro(name, {key: KeyCode.SPACE});
      case MacroName.KEY_PRESS_DOWN:
        return new KeyPressMacro(name, {key: KeyCode.DOWN});
      case MacroName.KEY_PRESS_LEFT:
        return new KeyPressMacro(name, {key: KeyCode.LEFT});
      case MacroName.KEY_PRESS_RIGHT:
        return new KeyPressMacro(name, {key: KeyCode.RIGHT});
      case MacroName.KEY_PRESS_UP:
        return new KeyPressMacro(name, {key: KeyCode.UP});
      case MacroName.KEY_PRESS_TOGGLE_OVERVIEW:
        // The MEDIA_LAUNCH_APP1 key is bound to the kToggleOverview accelerator
        // action in accelerators.cc.
        return new KeyPressMacro(name, {key: KeyCode.MEDIA_LAUNCH_APP1});
      case MacroName.KEY_PRESS_MEDIA_PLAY_PAUSE:
        return new KeyPressMacro(name, {key: KeyCode.MEDIA_PLAY_PAUSE});
      case MacroName.KEY_PRESS_SCREENSHOT:
        return new KeyPressMacro(name, {key: KeyCode.SNAPSHOT});
      case MacroName.OPEN_FACEGAZE_SETTINGS:
        return new CustomCallbackMacro(MacroName.OPEN_FACEGAZE_SETTINGS, () => {
          chrome.accessibilityPrivate.openSettingsSubpage(
              GestureHandler.SETTINGS_PATH);
        });
      case MacroName.TOGGLE_FACEGAZE:
        return new CustomCallbackMacro(
            MacroName.TOGGLE_FACEGAZE,
            () => {
              this.mouseController_.togglePaused();
              this.togglePaused(gesture);
            },
            /*toggleDirection=*/ this.paused_ ? ToggleDirection.ON :
                                                ToggleDirection.OFF);
      case MacroName.TOGGLE_SCROLL_MODE:
        return new MouseScrollMacro(this.mouseController_);
      case MacroName.TOGGLE_VIRTUAL_KEYBOARD:
        return new CustomCallbackMacro(
            MacroName.TOGGLE_VIRTUAL_KEYBOARD, async () => {
              // TODO(b/355662617): Unify with SwitchAccessPredicate.
              const isVisible = (node: AutomationNode): boolean => {
                return Boolean(
                    !node.state![StateType.OFFSCREEN] && node.location &&
                    node.location.top >= 0 && node.location.left >= 0 &&
                    !node.state![StateType.INVISIBLE]);
              };

              const desktop = await AsyncUtil.getDesktop();
              const keyboard = desktop.find({role: RoleType.KEYBOARD});
              const currentlyVisible = Boolean(
                  keyboard && isVisible(keyboard) &&
                  keyboard.find({role: RoleType.ROOT_WEB_AREA}));
              // Toggle the visibility of the virtual keyboard.
              chrome.accessibilityPrivate.setVirtualKeyboardVisible(
                  !currentlyVisible);
            });
      case MacroName.CUSTOM_KEY_COMBINATION:
        const keyCombination = this.gesturesToKeyCombos_.get(gesture);
        if (!keyCombination) {
          throw new Error(
              `Expected a custom key combination for gesture: ${gesture}`);
        }

        return new KeyPressMacro(name, keyCombination);
      default:
        return;
    }
  }

  private isMacroAllowed_(name: MacroName): boolean {
    if (this.isDictationActive_() && name !== MacroName.TOGGLE_DICTATION) {
      return false;
    }

    if (this.mouseController_.isScrollModeActive() &&
        name !== MacroName.TOGGLE_SCROLL_MODE) {
      return false;
    }

    if (this.paused_ && name !== MacroName.TOGGLE_FACEGAZE) {
      return false;
    }

    if (this.mouseController_.isLongClickActive() &&
        name !== MacroName.MOUSE_LONG_CLICK_LEFT) {
      return false;
    }

    return true;
  }
}

export namespace GestureHandler {
  /** The default confidence threshold for facial gestures. */
  export const DEFAULT_CONFIDENCE_THRESHOLD = 0.5;

  /** Minimum repeat rate of a gesture. */
  export const DEFAULT_REPEAT_DELAY_MS = 1000;

  export const GESTURE_TO_KEY_COMBO_PREF =
      'settings.a11y.face_gaze.gestures_to_key_combos';

  /**
   * Pref name of preference mapping facegaze gestures to macro action names.
   */
  export const GESTURE_TO_MACRO_PREF =
      'settings.a11y.face_gaze.gestures_to_macros';

  export const GESTURE_TO_CONFIDENCE_PREF =
      'settings.a11y.face_gaze.gestures_to_confidence';

  export const SETTINGS_PATH = 'manageAccessibility/faceGaze';
}

TestImportManager.exportForTesting(GestureHandler);
