// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_data.h"
#include "net/base/isolation_info.h"
#include "ui/accessibility/ax_tree_id.h"
#include "url/origin.h"

namespace autofill {

class FormStructure;

// AutofillDriver is Autofill's lowest-level abstraction of a frame that is
// shared among all platforms.
//
// Most notably, it is a gateway for all communication from the browser code to
// the DOM, or more precisely, to the AutofillAgent (which are different classes
// of the same name in non-iOS vs iOS).
//
// The reverse communication, from the AutofillAgent to the browser code, goes
// through mojom::AutofillDriver on non-iOS, and directly to AutofillManager on
// iOS.
//
// An AutofillDriver corresponds to a frame, rather than a document, in the
// sense that it may survive navigations.
//
// AutofillDriver has two implementations:
// - AutofillDriverIOS for iOS,
// - ContentAutofillDriver for all other platforms.
//
// An AutofillDriver's lifetime should be contained by the associated frame.
//
// AutofillDriverIOS is co-owned by AutofillDriverIOSWebFrame, which itself is
// owned by the WebFrame, and the iOS-specific AutofillAgent, which extends the
// driver's lifetime beyond the WebFrame's (crbug.com/892612).
//
// ContentAutofillDriver is owned by ContentAutofillDriverFactory, which ensures
// that the associated RenderFrameHost's lifetime contains the driver's
// lifetime.
class AutofillDriver {
 public:
  virtual ~AutofillDriver() = default;

  // Returns whether the AutofillDriver instance is associated with an active
  // frame in the MPArch sense.
  virtual bool IsInActiveFrame() const = 0;

  // Returns whether the AutofillDriver instance is associated with a main
  // frame, in the MPArch sense. This can be a primary or non-primary main
  // frame.
  virtual bool IsInAnyMainFrame() const = 0;

  // Returns whether the AutofillDriver instance is associated with a
  // prerendered frame.
  virtual bool IsPrerendering() const = 0;

  // Returns true iff a popup can be shown on the behalf of the associated
  // frame.
  virtual bool CanShowAutofillUi() const = 0;

  // Sets whether the keyboard should be suppressed. Used to keep the keyboard
  // hidden while the bottom sheet (e.g. Touch To Fill) is shown. Forwarded to
  // the last-queried source remembered by `ContentAutofillRouter`.
  virtual void SetShouldSuppressKeyboard(bool suppress) = 0;

  // Triggers a reparse on all frames of the same frame tree. Calls
  // `trigger_reparse_finished_callback` when all frames reported back being
  // done. If `success == false`, reparse was triggered while another reparse
  // was ongoing.
  virtual void TriggerReparseInAllFrames(
      base::OnceCallback<void(bool success)>
          trigger_reparse_finished_callback) = 0;

  // Returns the ax tree id associated with this driver.
  virtual ui::AXTreeID GetAxTreeId() const = 0;

  // Returns true iff the renderer is available for communication.
  virtual bool RendererIsAvailable() = 0;

  // Forwards |data| to the renderer which shall preview or fill the values of
  // |data|'s fields into the relevant DOM elements.
  //
  // |action| is the action the renderer should perform with the |data|.
  //
  // |triggered_origin| is the origin of the field on which Autofill was
  // triggered, and |field_type_map| contains the type predictions of the fields
  // that may be previewed or filled; these two parameters can be taken into
  // account to decide which fields to preview or fill across frames. See
  // FormForest::GetRendererFormsOfBrowserForm() for the details on Autofill's
  // security policy.
  //
  // Returns the ids of those fields that are safe to fill according to the
  // security policy for cross-frame previewing and filling. This is a subset of
  // the ids of the fields in |data|. It is not necessarily a subset of the
  // fields in |field_type_map|.
  //
  // This method is a no-op if the renderer is not currently available.
  virtual std::vector<FieldGlobalId> FillOrPreviewForm(
      mojom::RendererFormDataAction action,
      const FormData& data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map) = 0;

  // Forwards parsed |forms| to the embedder.
  virtual void HandleParsedForms(const std::vector<FormData>& forms) = 0;

  // Sends the field type predictions specified in |forms| to the renderer. This
  // method is a no-op if the renderer is not available or the appropriate
  // command-line flag is not set.
  virtual void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) = 0;

  // Tells the renderer to accept data list suggestions for |value|.
  virtual void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field_id,
      const std::u16string& value) = 0;

  // Tells the renderer to clear the current section of the autofilled values.
  virtual void RendererShouldClearFilledSection() = 0;

  // Tells the renderer to clear the currently previewed Autofill results.
  virtual void RendererShouldClearPreviewedForm() = 0;

  // Tells the renderer to set the node text.
  virtual void RendererShouldFillFieldWithValue(
      const FieldGlobalId& field_id,
      const std::u16string& value) = 0;

  // Tells the renderer to preview the node with suggested text.
  virtual void RendererShouldPreviewFieldWithValue(
      const FieldGlobalId& field_id,
      const std::u16string& value) = 0;

  // Tells the renderer to set the currently focused node's corresponding
  // accessibility node's autofill state to |state|.
  virtual void RendererShouldSetSuggestionAvailability(
      const FieldGlobalId& field_id,
      const mojom::AutofillState state) = 0;

  // Informs the renderer that the popup has been hidden.
  virtual void PopupHidden() = 0;

  virtual net::IsolationInfo IsolationInfo() = 0;

  // Tells the renderer about the form fields that are eligible for triggering
  // manual filling on form interaction.
  virtual void SendFieldsEligibleForManualFillingToRenderer(
      const std::vector<FieldGlobalId>& fields) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_H_
