// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_tracker.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/modules/autofill/web_form_element_observer.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/base/page_transition_types.h"

using blink::WebDocumentLoader;
using blink::WebInputElement;
using blink::WebFormControlElement;
using blink::WebFormElement;

namespace autofill {

using mojom::SubmissionSource;

FormTracker::FormTracker(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      blink::WebLocalFrameObserver(render_frame->GetWebFrame()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
}

FormTracker::~FormTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormTracker::AddObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  observers_.AddObserver(observer);
}

void FormTracker::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  observers_.RemoveObserver(observer);
}

void FormTracker::AjaxSucceeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  FireSubmissionIfFormDisappear(SubmissionSource::XHR_SUCCEEDED);
}

void FormTracker::TextFieldDidChange(const WebFormControlElement& element) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  DCHECK(!element.DynamicTo<WebInputElement>().IsNull() ||
         form_util::IsTextAreaElement(element));

  if (ignore_control_changes_)
    return;

  // If the element isn't focused then the changes don't matter. This check is
  // required to properly handle IME interactions.
  if (!element.Focused())
    return;

  const WebInputElement input_element = element.DynamicTo<WebInputElement>();
  if (input_element.IsNull())
    return;

  // Disregard text changes that aren't caused by user gestures or pastes. Note
  // that pastes aren't necessarily user gestures because Blink's conception of
  // user gestures is centered around creating new windows/tabs.
  if (user_gesture_required_ &&
      !render_frame()->GetWebFrame()->HasTransientUserActivation() &&
      !render_frame()->IsPasting())
    return;

  // We post a task for doing the Autofill as the caret position is not set
  // properly at this point (http://bugs.webkit.org/show_bug.cgi?id=16976) and
  // it is needed to trigger autofill.
  weak_ptr_factory_.InvalidateWeakPtrs();
  render_frame()
      ->GetWebFrame()
      ->GetTaskRunner(blink::TaskType::kInternalUserInteraction)
      ->PostTask(FROM_HERE,
                 base::BindRepeating(
                     &FormTracker::FormControlDidChangeImpl,
                     weak_ptr_factory_.GetWeakPtr(), element,
                     Observer::ElementChangeSource::TEXTFIELD_CHANGED));
}

void FormTracker::SelectControlDidChange(const WebFormControlElement& element) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);

  if (ignore_control_changes_)
    return;

  // Post a task to avoid processing select control change while it is changing.
  weak_ptr_factory_.InvalidateWeakPtrs();
  render_frame()
      ->GetWebFrame()
      ->GetTaskRunner(blink::TaskType::kInternalUserInteraction)
      ->PostTask(FROM_HERE, base::BindRepeating(
                                &FormTracker::FormControlDidChangeImpl,
                                weak_ptr_factory_.GetWeakPtr(), element,
                                Observer::ElementChangeSource::SELECT_CHANGED));
}

void FormTracker::TrackAutofilledElement(const WebFormControlElement& element) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  DCHECK(element.IsAutofilled());

  if (ignore_control_changes_)
    return;

  ResetLastInteractedElements();
  if (element.Form().IsNull())
    last_interacted_formless_element_ = element;
  else
    last_interacted_form_ = element.Form();
  TrackElement();
}

void FormTracker::FireProbablyFormSubmittedForTesting() {
  FireProbablyFormSubmitted();
}

void FormTracker::FormControlDidChangeImpl(
    const WebFormControlElement& element,
    Observer::ElementChangeSource change_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  // The frame or document could be null because this function is called
  // asynchronously.
  const blink::WebDocument& doc = element.GetDocument();
  if (!render_frame() || doc.IsNull() || !doc.GetFrame())
    return;

  if (element.Form().IsNull()) {
    last_interacted_formless_element_ = element;
  } else {
    last_interacted_form_ = element.Form();
  }

  for (auto& observer : observers_) {
    observer.OnProvisionallySaveForm(element.Form(), element, change_source);
  }
}

void FormTracker::DidCommitProvisionalLoad(ui::PageTransition transition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormTracker::DidFinishSameDocumentNavigation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  FireSubmissionIfFormDisappear(SubmissionSource::SAME_DOCUMENT_NAVIGATION);
}

void FormTracker::DidStartNavigation(
    const GURL& url,
    absl::optional<blink::WebNavigationType> navigation_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  blink::WebLocalFrame* navigated_frame = render_frame()->GetWebFrame();
  // Ony handle primary main frame.
  if (!navigated_frame->IsOutermostMainFrame())
    return;

  // Bug fix for crbug.com/368690. isProcessingUserGesture() is false when
  // the user is performing actions outside the page (e.g. typed url,
  // history navigation). We don't want to trigger saving in these cases.

  // We are interested only in content-initiated navigations. Explicit browser
  // initiated navigations (e.g. via omnibox) don't have a navigation type
  // and are discarded here.
  if (navigation_type.has_value() &&
      navigation_type.value() != blink::kWebNavigationTypeLinkClicked) {
    FireProbablyFormSubmitted();
  }
}

void FormTracker::WillDetach() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  FireInferredFormSubmission(SubmissionSource::FRAME_DETACHED);
}

void FormTracker::WillSendSubmitEvent(const WebFormElement& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  last_interacted_form_ = form;
  for (auto& observer : observers_) {
    observer.OnProvisionallySaveForm(
        form, blink::WebFormControlElement(),
        Observer::ElementChangeSource::WILL_SEND_SUBMIT_EVENT);
  }
}

void FormTracker::WillSubmitForm(const WebFormElement& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);

  // A form submission may target a frame other than the frame that owns |form|.
  // The WillSubmitForm() event is only fired on the target frame's FormTracker
  // (provided that both have the same origin). In such a case, we ignore the
  // form submission event. If we didn't, we would send |form| to an
  // AutofillAgent and then to a ContentAutofillDriver etc. which haven't seen
  // this form before. See crbug.com/1240247#c13 for details.
  if (!form_util::IsOwnedByFrame(form, render_frame()))
    return;

  FireFormSubmitted(form);
}

void FormTracker::OnDestruct() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormTracker::OnFrameDetached() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormTracker::FireFormSubmitted(const blink::WebFormElement& form) {
  for (auto& observer : observers_)
    observer.OnFormSubmitted(form);
  ResetLastInteractedElements();
}

void FormTracker::FireProbablyFormSubmitted() {
  if (base::FeatureList::IsEnabled(
          features::kAutofillProbableFormSubmissionInBrowser)) {
    return;
  }

  for (auto& observer : observers_)
    observer.OnProbablyFormSubmitted();
  ResetLastInteractedElements();
}

void FormTracker::FireInferredFormSubmission(SubmissionSource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  for (auto& observer : observers_)
    observer.OnInferredFormSubmission(source);
  ResetLastInteractedElements();
}

void FormTracker::FireSubmissionIfFormDisappear(SubmissionSource source) {
  if (CanInferFormSubmitted()) {
    FireInferredFormSubmission(source);
    return;
  }
  TrackElement();
}

bool FormTracker::CanInferFormSubmitted() {
  // If last interacted form is available, assume form submission only if the
  // form is now gone, either invisible or removed from the DOM.
  // Otherwise (i.e., there is no form tag), we check if the last element the
  // user has interacted with are gone, to decide if submission has occurred.
  if (!last_interacted_form_.IsNull()) {
    return !base::ranges::any_of(last_interacted_form_.GetFormControlElements(),
                                 &form_util::IsWebElementFocusable);
  } else if (!last_interacted_formless_element_.IsNull())
    return !form_util::IsWebElementFocusable(last_interacted_formless_element_);

  return false;
}

void FormTracker::TrackElement() {
  // Already has observer for last interacted element.
  if (form_element_observer_)
    return;
  auto callback = base::BindOnce(&FormTracker::ElementWasHiddenOrRemoved,
                                 base::Unretained(this));

  if (!last_interacted_form_.IsNull()) {
    form_element_observer_ = blink::WebFormElementObserver::Create(
        last_interacted_form_, std::move(callback));
  } else if (!last_interacted_formless_element_.IsNull()) {
    form_element_observer_ = blink::WebFormElementObserver::Create(
        last_interacted_formless_element_, std::move(callback));
  }
}

void FormTracker::ResetLastInteractedElements() {
  last_interacted_form_.Reset();
  last_interacted_formless_element_.Reset();
  if (form_element_observer_) {
    form_element_observer_->Disconnect();
    form_element_observer_ = nullptr;
  }
}

void FormTracker::ElementWasHiddenOrRemoved() {
  FireInferredFormSubmission(SubmissionSource::DOM_MUTATION_AFTER_XHR);
}

}  // namespace autofill
