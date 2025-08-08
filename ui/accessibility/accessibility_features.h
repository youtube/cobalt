// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define all the base::Features used by ui/accessibility.
#ifndef UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
#define UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_base_export.h"

namespace features {

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityAriaVirtualContent);

// Returns true if "aria-virtualcontent" should be recognized as a valid aria
// property.
AX_BASE_EXPORT bool IsAccessibilityAriaVirtualContentEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityExposeHTMLElement);

// Returns true if the <html> element should be exposed to the
// browser process AXTree (as an ignored node).
AX_BASE_EXPORT bool IsAccessibilityExposeHTMLElementEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityExposeIgnoredNodes);

// Returns true if all ignored nodes are exposed by Blink in the
// accessibility tree.
AX_BASE_EXPORT bool IsAccessibilityExposeIgnoredNodesEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityLanguageDetection);

// Return true if language detection should be used to determine the language
// of text content in page and exposed to the browser process AXTree.
AX_BASE_EXPORT bool IsAccessibilityLanguageDetectionEnabled();

// Serializes accessibility information from the Views tree and deserializes it
// into an AXTree in the browser process.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityTreeForViews);

// Returns true if the Views tree is exposed using an AXTree in the browser
// process. Returns false if the Views tree is exposed to accessibility
// directly.
AX_BASE_EXPORT bool IsAccessibilityTreeForViewsEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAccessibilityRestrictiveIA2AXModes);

// Returns true if the more restrictive approach that only enables the web
// content related AXModes on an IA2 query when the data is being queried on an
// web content node.
//
// TODO(1441211): Remove flag once the change has been confirmed safe.
AX_BASE_EXPORT bool IsAccessibilityRestrictiveIA2AXModesEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityFocusHighlight);

// Returns true if the accessibility focus highlight feature is enabled,
// which draws a visual highlight around the focused element on the page
// briefly whenever focus changes.
AX_BASE_EXPORT bool IsAccessibilityFocusHighlightEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAutoDisableAccessibility);

// Returns true if accessibility will be auto-disabled after a certain
// number of user input events spanning a minimum amount of time with no
// accessibility API usage in that time.
AX_BASE_EXPORT bool IsAutoDisableAccessibilityEnabled();

// Enables a setting that can turn on/off browser vocalization of 'descriptions'
// tracks.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kTextBasedAudioDescription);

// Returns true if the setting to turn on text based audio descriptions is
// enabled.
AX_BASE_EXPORT bool IsTextBasedAudioDescriptionEnabled();

#if BUILDFLAG(IS_WIN)
// Enables an experimental Chrome-specific accessibility COM API
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kIChromeAccessible);

// Returns true if the IChromeAccessible COM API is enabled.
AX_BASE_EXPORT bool IsIChromeAccessibleEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kSelectiveUIAEnablement);

// Returns true if accessibility will be selectively enabled depending on the
// UIA APIs that are called, allowing non-screenreader usage to enable less of
// the accessibility system.
AX_BASE_EXPORT bool IsSelectiveUIAEnablementEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kUiaProvider);

// Returns true if the browser's UIA provider should be used when requested by
// an a11y client.
AX_BASE_EXPORT bool IsUiaProviderEnabled();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
AX_BASE_EXPORT bool IsDictationOfflineAvailable();

// Enables Context Checking with the accessibility Dictation feature.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kExperimentalAccessibilityDictationContextChecking);

// Returns true if Dictation with context checking is enabled.
AX_BASE_EXPORT bool
IsExperimentalAccessibilityDictationContextCheckingEnabled();

// Enables downloading Google TTS voices using Language Packs.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kExperimentalAccessibilityGoogleTtsLanguagePacks);

// Returns true if using Language Packs to download Google TTS voices is
// enabled.
AX_BASE_EXPORT bool IsExperimentalAccessibilityGoogleTtsLanguagePacksEnabled();

// Enables downloading Google TTS High Quality voices.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kExperimentalAccessibilityGoogleTtsHighQualityVoices);

// Returns true if downloading High Quality Google TTS voices is enabled.
AX_BASE_EXPORT bool
IsExperimentalAccessibilityGoogleTtsHighQualityVoicesEnabled();

// Enables Dictation keyboard improvements.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kAccessibilityDictationKeyboardImprovements);

// Returns true if Dictation keyboard improvements are enabled.
AX_BASE_EXPORT bool IsAccessibilityDictationKeyboardImprovementsEnabled();

// Enables AccessibilitySelectToSpeakHoverTextImprovements.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kAccessibilitySelectToSpeakHoverTextImprovements);

// Returns true if AccessibilitySelectToSpeakHoverTextImprovements is enabled.
AX_BASE_EXPORT bool IsAccessibilitySelectToSpeakHoverTextImprovementsEnabled();

// Enables accessibility accelerator notifications to timeout.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(
    kAccessibilityAcceleratorNotificationsTimeout);

// Returns true if kAccessibilityAcceleratorNotificationsTimeout is enabled.
AX_BASE_EXPORT bool IsAccessibilityAcceleratorNotificationsTimeoutEnabled();

// Enables the experimental GameFace integration.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityGameFaceIntegration);

// Returns true if the GameFace integration is enabled.
AX_BASE_EXPORT bool IsAccessibilityGameFaceIntegrationEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// A feature that makes PDFs displayed in the ChromeOS Media App (AKA Backlight)
// accessible by performing OCR on the images for each page.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kBacklightOcr);
AX_BASE_EXPORT bool IsBacklightOcrEnabled();

// Enables Get Image Descriptions to augment existing images labels,
// rather than only provide descriptions for completely unlabeled images.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAugmentExistingImageLabels);

// Returns true if augmenting existing image labels is enabled.
AX_BASE_EXPORT bool IsAugmentExistingImageLabelsEnabled();

// Once this flag is enabled, a single codebase in AXPosition will be used for
// handling document markers on all platforms, including the announcement of
// spelling mistakes.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kUseAXPositionForDocumentMarkers);

// Returns true if document markers are exposed on inline text boxes in the
// accessibility tree in addition to on static text nodes. This in turn enables
// AXPosition on the browser to discover and work with document markers, instead
// of the legacy code that collects document markers manually from static text
// nodes and which is different for each platform.
AX_BASE_EXPORT bool IsUseAXPositionForDocumentMarkersEnabled();

// Enable support for ARIA element reflection, for example
// element.ariaActiveDescendantElement = child;
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kEnableAriaElementReflection);

// Returns true if ARIA element reflection is enabled.
AX_BASE_EXPORT bool IsAriaElementReflectionEnabled();

// Experiment to increase the cost of SendPendingAccessibilityEvents.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAblateSendPendingAccessibilityEvents);

// Returns true if |kAblateSendPendingAccessibilityEvents| is enabled.
AX_BASE_EXPORT bool IsAblateSendPendingAccessibilityEventsEnabled();

#if BUILDFLAG(IS_ANDROID)
// Enable filtered AXModes based on running services. If disabled, then AXModes
// will not be available to be set.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityPerformanceFiltering);

// Returns true if AXMode filtering for performance is enabled.
AX_BASE_EXPORT bool IsAccessibilityPerformanceFilteringEnabled();

#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnything);

// Returns true if read anything is enabled. This feature shows users websites,
// such as articles, in a comfortable reading experience in a side panel.
AX_BASE_EXPORT bool IsReadAnythingEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingWithScreen2x);

// Returns true if read anything is enabled with screen2x integration, which
// distills web pages using an ML model.
AX_BASE_EXPORT bool IsReadAnythingWithScreen2xEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kDataCollectionModeForScreen2x);

// If enabled, the browser will open with read_anything open in the side panel,
// and calls distill only once we receive navigation's load-complete event.
// This is because the browser is only being opened to render one webpage, for
// the sake of generating training data for Screen2x data collection. The
// browser is intended to be closed by the user who launches Chrome once the
// first distill call finishes executing. This feature should be used along
// with 'ScreenAIDebugModeEnabled=true' and --no-sandbox.
AX_BASE_EXPORT bool IsDataCollectionModeForScreen2xEnabled();

// If enabled, ScreenAI library writes some debug data in /tmp.
AX_BASE_EXPORT bool IsScreenAIDebugModeEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingWebUIToolbar);

// If enabled, use the WebUI toolbar in Read Anything.
AX_BASE_EXPORT bool IsReadAnythingWebUIToolbarEnabled();

AX_BASE_EXPORT BASE_DECLARE_FEATURE(kReadAnythingReadAloud);

// If enabled, show the Read Aloud feature in Read Anything.
AX_BASE_EXPORT bool IsReadAnythingReadAloudEnabled();

// Enables a feature whereby inaccessible (i.e. untagged) PDFs are made
// accessible using an optical character recognition service. Due to the size of
// the OCR component, this feature targets desktop versions of Chrome for now.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kPdfOcr);

// Returns true if OCR will be performed on inaccessible (i.e. untagged) PDFs
// and the resulting text, together with its layout information, will be added
// to the accessibility tree.
AX_BASE_EXPORT bool IsPdfOcrEnabled();

// Enables a feature whereby inaccessible surfaces such as canvases are made
// accessible using a local machine intelligence service.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kLayoutExtraction);

// Returns true if Layout Extraction feature is enabled. This feature uses a
// local machine intelligence library to process screenshots and adds metadata
// to the accessibility tree.
AX_BASE_EXPORT bool IsLayoutExtractionEnabled();

// Enables the experimental Accessibility Service.
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityService);

// Returns true if the Accessibility Service enabled.
AX_BASE_EXPORT bool IsAccessibilityServiceEnabled();

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
// Enables the NSAccessibilityRemoteUIElement's RemoteUIApp. We need to set
// NSAccessibilityRemoteUIElement's RemoteUIApp to YES, which is to fix some
// a11y bugs in PWA Mac.
// It is a temporary feature flag, and will be removed once browser with this
// feature enabled can run stably. The reason we're so careful is because a
// previous CL that enabling NSAccessibilityRemoteUIElement's RemoteUIApp caused
// chromium to hang. With the feature flag, once chromium encounters a bug due
// to this we can urgently disable it. See https://crbug.com/1491329
AX_BASE_EXPORT BASE_DECLARE_FEATURE(kAccessibilityRemoteUIApp);

// Returns true if the NSAccessibilityRemoteUIElement's RemoteUIApp is enabled.
AX_BASE_EXPORT bool IsAccessibilityRemoteUIAppEnabled();

#endif  // BUILDFLAG(IS_MAC)

}  // namespace features

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
