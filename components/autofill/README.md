# Autofill

Autofill is a [layered
component](https://sites.google.com/a/chromium.org/dev/developers/design-documents/layered-components-design).
It has the following structure:

- [`core/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core): Code shared by `content/` and `ios/`.
  - [`browser/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser): Browser process code.
  - [`common/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/common): Code shared by the browser and the renderer.
- [`content/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content): Driver using the `//content` layer (all platforms except iOS).
  - [`browser/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/browser): Browser process code.
  - [`renderer/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/renderer): Renderer process code.
  - [`common/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/common): Code shared by the browser and the renderer.
- [`ios/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios): Driver using `//ios` (as opposed to `//content`).
- [`android/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/android): Java code for Android.

## High-level architecture

The following architecture diagram shows instances of Autofill's core objects.
For simplicity, we only consider the case of Chrome Autofill on non-iOS
platforms. The diagram is best read bottom-up because every Autofill flow starts
with `AutofillAgent` extracting a form from the DOM.

```
┌────────────────────┐
│PersonalDataManager ├────────┬──────────────────┐
│1 per BrowserContext│  owns N│            owns N│
└─▲──────────────────┘      ┌─▼─────────────┐  ┌─▼────────┐
  │                         │AutofillProfile│  │CreditCard│
  │weak ref                 └───────────────┘  └──────────┘
┌─┴───────────────┐
│FormDataImporter ◄─────────────────────┐
│1 per WebContents│               events│
└─▲───────────────┘                     │
  │                                     │
  │ ┌────────────────────────┐        ┌─┴────────────────────┐
  │ │AutofillExternalDelegate◄────────┤BrowserAutofillManager├───────┐     ┌───────────────┐
  │ │1 per RenderFrameHost   │  owns 1│1 per RenderFrameHost │  votes│     │Autofill server│
  │ └──────────────────────┬─┘        └─▲──────────────────┬─┘       │     └─────────────▲─┘
  │                  events│            │            events│         │               HTTP│
  │                        │            │                  │       ┌─▼───────────────────▼─┐
  ├────────────────────────┼────────────┼──────────────────┼───────►AutofillDownloadManager│
  │                        │            │                  │       │1 per WebContents      │
  │                        │            │                  │       └─────────────────────▲─┘
  │                        │            │                  │                             │
  │                        │            │                  │    ┌──────────────┐         │
  │                        │            │                  │    │FormStructure │         │
  │                        │            │                  │    │1 per FormData│         │
  │                        │            │                  └──┐ └─▲────────────┘         │
  │owns 1                  │            │events               │   │sets types            │
┌─┴──────────────────┐     │          ┌─┴───────────────────┐ │   │owns N         queries│
│ChromeAutofillClient◄─────┼──────────┤AutofillManager      ├─┼───┴──────────────────────┘
│1 per WebContents   │     │  weak ref│1 per RenderFrameHost│ │
└─┬──────────────────┘     │          └─▲─────────────────┬─┘ │
  │owns 1                  │            │           events│   │
  │                        └────────────┼────────────────►│◄──┘
  │                                     │                 │
  │                        ┌────────────┼─────────────────┼────────────┐
  │                        │owns 1      │events           │            │
  │                        │            │owns 1           │            │
┌─▼────────────────────────┴─┐        ┌─┴─────────────────▼─┐        ┌─▼───────────────────┐
│ContentAutofillDriverFactory├────────►ContentAutofillDriver◄────────►ContentAutofillRouter│
│1 per WebContents           │owns N  │1 per RenderFrameHost│ events │1 per WebContents    │
└────────────────────────────┘        └─▲─────────┬─────────┘        └─────────────────────┘
                                        │         │fill form and
Browser                                 │         │other events
1 process                               │         │
────────────────────────────────────────┼─────────┼─────────────────────────────────────────
Renderer                                │         │
N processes           events, often with│         │
                      FormData objects  │         │
                                      ┌─┴─────────▼─────┐       ┌─────────────────────┐
                                      │AutofillAgent    ├───────►form_autofill_util.cc│
                                      │1 per RenderFrame│calls  └─────────────────────┘
                                      └─────────────────┘
```
To edit the diagram, copy-paste it to asciiflow.com.

A [`WebContents`](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/web_contents.h)
corresponds to a tab. A [`RenderFrameHost`](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/render_frame_host.h)
roughly corresponds to a frame or a document (but to neither exactly; they
differ in whether or not they survive navigations; details are [here](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/render_document.md)
and [here](https://docs.google.com/document/d/1C2VKkFRSc0kdmqjKan1G4NlNlxWZqE4Wam41FNMgnmA/edit#)).
A [`BrowserContext`](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/browser_context.h)
corresponds to a [`Profile`](https://www.chromium.org/developers/design-documents/profile-architecture/).

### Differences among platforms and embedders

* Desktop vs Android: Android has different UI, which mostly lives in
  `//chrome`.
* non-iOS vs iOS: iOS also uses `AutofillManager` and everything north of it,
  but `AutofillDriverIOS*` instead of `ContentAutofill*`, and a different but
  identically named `AutofillAgent`.
* Chrome vs WebView: WebView also uses `AutofillManager` and everything south
  of it, but `AwAutofillClient` instead of `ChromeAutofillClient`, and
  `AndroidAutofillManager` instead of `BrowserAutofillManager`.

### Links to files

- [`core/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core)
  - [`common/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/common)
    - [`form_data.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/common/form_data.h)
  - [`browser/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser)
    - [`autofill_client.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/autofill_client.h)
      - [`//android_webview/browser/aw_autofill_client.h`](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/browser/aw_autofill_client.h) (WebView implementation)
      - [`//chrome/browser/ui/autofill/chrome_autofill_client.h`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/autofill/chrome_autofill_client.h) (Chrome implementation)
    - [`autofill_download_manager.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/autofill_download_manager.h)
    - [`autofill_driver.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/autofill_driver.h)
      - [`../../content/browser/content_autofill_driver.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/autofill_driver.h) (non-iOS implementation)
      - [`../../ios/browser/autofill_driver_ios.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/browser/autofill_driver_ios.h) (iOS implementation)
    - [`autofill_external_delegate.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/autofill_external_delegate.h)
    - [`autofill_manager.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/autofill_manager.h)
      - [`browser_autofill_manager.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/browser_autofill_manager.h) (Chrome specialization)
      - [`//components/android_autofill/browser/android_autofill_manager.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/android_autofill/browser/android_autofill_manager.h) (WebView specialization)
    - [`data_model/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/data_model)
      - [`autofill_profile.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/data_model/autofill_profile.h)
      - [`credit_card.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/data_model/credit_card.h)
    - [`form_data_importer.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/form_data_importer.h)
    - [`form_structure.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/form_structure.h)
    - [`personal_data_manager.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/personal_data_manager.h)
    - [`proto/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/proto/) (Autofill server)
- [`content/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content)
  - [`browser/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/browser)
      - [`content_autofill_driver.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/browser/content_autofill_driver.h)
      - [`content_autofill_driver_factory.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/browser/content_autofill_driver_factory.h)
      - [`content_autofill_router.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/browser/content_autofill_router.h)
  - [`renderer/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/renderer)
      - [`autofill_agent.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/renderer/autofill_agent.h)
      - [`form_autofill_util.cc`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/renderer/autofill_agent.h)
- [`ios/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios)
  - [`browser/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/browser)
    - [`autofill_agent.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/browser/autofill_agent.h)
    - [`autofill_driver_ios.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/browser/autofill_driver_ios.h)
  - [`form_util/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/form_util)
    - [`form.js`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/form_util/resources/form.js)
    - [`fill.js`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/form_util/resources/fill.js)

# Related directories

There are some closely related directories in `//chrome`:

- [`//chrome/browser/autofill`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/autofill)
- [`//chrome/browser/ui/android/autofill`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/autofill)
  - [`//chrome/android/java/src/org/chromium/chrome/browser/autofill`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/autofill/)
- [`//chrome/browser/ui/autofill`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/autofill)
- [`//chrome/browser/ui/views/autofill`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/autofill)

# Autofill cheatsheet

This is a cheatsheet to navigate Autofill. It is not necessarily exhaustive and
may sacrifice a little bit of correctness in favor of simplicity.

## What are the main classes that orchestrate Autofill?

* Renderer:
  * `AutofillAgent`
    * One instance per `RenderFrame` (frame).
    * Responsibilities:
      * Observes forms in a frame and notifies the browser about changes.
      * Executes preview and filling requests.
    * Implements `blink::WebAutofillClient` to communicate with Blink.
* Browser:
  * `ContentAutofillDriver`
    * One instance per `RenderFrameHost` (frame), owned by
      `ContentAutofillDriverFactory`
    * Responsibilities:
      * Facilitates communication between the browser and the renderer logic.
    * Implements interfaces `AutofillDriver` and `mojom::AutofillDriver`
    * Has sibling `AutofillDriverIOS` for iOS
  * `ContentAutofillDriverFactory`
    * One instance per `WebContents` (tab)
    * Responsibilities:
      * Manages life-cycle of `ContentAutofillDriver` and ensures that there is
        one Driver instance per renderer frame.
    * Has sibling `AutofillDriverIOSFactory` for iOS
  * `AutofillManager` and `BrowserAutofillManager`
    * One instance per `RenderFrameHost` (frame), owned by
      `AutofillDriver`.
    * Responsibilities:
      * Main orchestrator for Autofill logic.
    * `BrowserAutofillManager` extends the `AutofillManager` base class.
    * `BrowserAutofillManager` has sibling `AndroidAutofillManager` which is
      responsible for Android Autofill for WebViews.
  * `ChromeAutofillClient`
    * One instance per `WebContents` (tab)
    * Responsibilities:
      * Serves as bridge from platform aganostic `BrowserAutofillManager` to the
        OS specific logic.
    * Implements `AutofillClient` interface.
    * Has siblings `AwAutofillClient`, `ChromeAutofillClientIOS` and
      `WebViewAutofillClientIOS`.

## What's the difference between Autofill and Autocomplete?

* Autofill is about structured and typed information (addresses, credit card
  data, ...)
* Autocomplete is about single-field, untyped information. Values are tied to a
  field identifier. Autocomplete is largely implemented by the
  `AutocompleteHistoryManager`.

## What are the representations for Forms and Fields?

* Between Renderer and Browser, we mostly exchange structural information
  * `FormData`
    * HTML attributes of a form
    * Contains a list of fields (`FormFieldData`).
    * 1:1 correspondence to a `blink::WebFormElement`
  * `FormFieldData`
    * HTML attributes of a field
    * current value (or value that should be filled)
    * `global_id()` gives a globally unique and non-changing identifier of a
      field in the renderer process.
    * 1:1 correspondence to a `blink::WebFormControlElement`
* On the Browser side, we have augmented information:
  * `FormStructure` - corresponds to a `FormData`
    * Container for a series of `FormFieldData` objects
  * `AutofillField` - corresponds to a `FormFieldData`
    * Inherits from `FormFieldData` and extends it by
      * Field type classifications
      * Other Autofill metadata

## How are forms and fields identified?
* Per page load, in particular for distinguishing DOM elements:
  * `FormGlobalId` is a pair of a `LocalFrameToken` and a `FormRendererId`,
     which uniquely identify the frame and the form element in that frame.
  * `FieldGlobalId` is a pair of a `LocalFrameToken` and a `FieldRendererId`.
* Across page loads, in particular for crowdsourcing:
  * `FormSignature` is a 64 bit hash value of the form URL (target URL or
    location of the embedding website as a fallback) and normalized field
    names.
  * `FieldSignature` is a 32 bit hash value of the field name (name attribute,
    falling back to id attribute of the form control) and type (text, search,
    password, tel, ...)

## How are field classified?
* Local heuristics
  * See `components/autofill/core/browser/form_parsing/`.
  * `FormField::ParseFormFields` is the global entry point for parsing fields
    with heuristics.
  * Local heuristics are only applied if a form has at least 3 fields and at
    least 3 fields are classified with distinct field types. There are
    exceptions for a few field types (email addresses, promo codes, IBANs, CVV
    fields).
  * We perform local heuristics even for smaller forms but only for promo codes
    and IBANs (see `ParseSingleFieldForms`).
  * Regular expressions for parsing are provided via
    `components/autofill/core/common/autofill_regex_constants.h` or
    `components/autofill/core/browser/form_parsing/regex_patterns.h` if
    `features::kAutofillParsingPatternProvider` is enabled.
* Crowd sourcing
  * `AutofillDownloadManager` is responsible for downloading field
    classifications.
  * Crowd sourcing is applied (for lookups and voting) for forms of any size but
    the server can handle small forms differently, see
    [`http://cs/IsSmallForm%20file:autofill`](http://cs/IsSmallForm%20file:autofill).
  * Crowd sourcing trumps local heuristics.
* Autocomplete attribute
  * The autocomplete attribute is parsed in `ParseAutocompleteAttribute`.
  * The autocomplete attribute trumps local heuristics and crowd sourcing
    (except for `off`).
* Rationalization
  * Rationalization is the process of looking at the output of the previous
    classification steps and doing a post processing for certain field
    combinations that don't make sense (street-address followed by
    address-line1).

## How is data represented internally?
* See `components/autofill/core/browser/data_model/`
  * For addresses, see
    `components/autofill/core/browser/data_model/autofill_structured_address.h`
    and
    `components/autofill/core/browser/data_model/autofill_structured_address_name.h`.
  * Parsing = breaking a bigger concept (e.g. street address) into smaller
    concepts (e.g. street name and house number). See
    `AddressComponent::ParseValueAndAssignSubcomponents()`.
      * Parsing goes through a chain until one method succeeds:
        * Via `ParseValueAndAssignSubcomponentsByMethod()`
        * Via `ParseValueAndAssignSubcomponentsByRegularExpressions()`
        * Finally `ParseValueAndAssignSubcomponentsByFallbackMethod()`
      * This is driven by the implementations of
        `GetParseRegularExpressionsByRelevance()`.
  * Formatting = combining the smaller concepts (e.g. street name and house
    number) into a bigger one (street address). See
    `AddressComponent::FormatValueFromSubcomponents()`.
    * This is driven by the implementations of `GetBestFormatString()`,
      in particular `StreetAddress::GetBestFormatString()`.
  * Invariance: The children of a node cannot contain more information than the
    parent node, or more more formally: every string in a node must be present
    in its parent (at least in a normalized form). If a subtree contains too
    much data, it is discarded via `AddressComponent::WipeInvalidStructure()`.

## Where is Autofill data persisted?
* See
  [`../../components/autofill/core/browser/webdata/autofill_table.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/webdata/autofill_table.h)

## What is a form submission?
The following situations are considered form submissions by Autofill and end up
at `AutofillManager::OnFormSubmitted()` with a submission source and an
assessment whether the form submission should be considered successful (meaning
that the website accepted the submitted values, not that the HTTP request
succeeded):

* A **regular HTTP form submission** (`FormTracker::WillSubmitForm()`).
  * Triggers `SubmissionSource::FORM_SUBMISSION` with `known_success=false`.
* A **main-frame navigation** was initiated in the content area but not triggered by
  a link click (`FormTracker::DidStartNavigation()`) - only if the frame has a
  `last_interacted_form_` or form-less element that the user interacted with.
  * Triggers `SubmissionSource::PROBABLY_FORM_SUBMITTED` with
    `known_success=false`.
* After a **same document navigation**
  (`FormTracker::DidFinishSameDocumentNavigation()`), the last interacted form
  is/becomes unfocusable or removed. The former condition is tested via
  `WebNode::IsFocusable()` and considers various styles (e.g. "display: none" on
  the node or a parent, "visibility: hidden") and attributes (e.g. "inert",
  tabindex="-1", "disabled") which prevent focusability.
  * Triggers `SubmissionSource::SAME_DOCUMENT_NAVIGATION` with
    `known_success=true`.
* After a **successful AJAX/XMLHttpRequest request**
  (`AutofillAgent::AjaxSucceeded()`), the last interacted form is/becomes
  unfocusable or removed.
  * Triggers `SubmissionSource::XHR_SUCCEEDED` if the form is already
    inaccessible or removed when the XHR succeeds (`known_success=true`).
  * Triggers `SubmissionSource::DOM_MUTATION_AFTER_XHR` if the form becomes
    inaccessible or removed after a successful XHR and the user does not
    interact with any other forms in between (`known_success=true`).
* The **subframe** or non-primary main frame containing the form was
  **detached** (`FormTracker::WillDetach()`)
  * Triggers `SubmissionSource::FRAME_DETACHED` with `known_success=true`.

## When are votes uploaded?
Autofill votes are theoretically uploaded
* when a **form is submitted**
  (`BrowserAutofillManager::OnFormSubmittedImpl()`).

  In this case `observed_submission=true` is passed to
  `BrowserAutofillManager::MaybeStartVoteUploadProcess`.
* when a the user **removes focus** from a form (this could happen because the
  user clicks on a custom autofill dropdown rendered by the website or if the
  user just clicks on the background).
  (`BrowserAutofillManager::OnFocusNoLongerOnFormImpl()` ->
  `BrowserAutofillManager::ProcessPendingFormForUpload()`).

  `observed_submission=false` is passed.
* when a the **form changes** (the structure, not the values) and we notice it
  (`BrowserAutofillManager::UpdatePendingForm()` ->
  `BrowserAutofillManager::ProcessPendingFormForUpload()`).

  `observed_submission=false` is passed.

In practice we allow only one upload per (form x submission source) every
`kAutofillUploadThrottlingPeriodInDays` days.

In case `observed_submission == true`, the votes are generated on a background
thread and then passed to the `AutofillDownloadManager`.

In case `observed_submission == false`, the votes are not directly passed to
the `AutofillDownloadManager`. Instead they are cached until the cache is
flushed. This enables us to override previous votes in case the user focuses
and removes focus from a form multiple times while editing the fields' values.
The cache is flushed on form submission.

As the votes generation is asynchronous, it is not guaranteed that the results
are available by the time the upload cache is flushed. In this case, votes are
only uploaded on the next navigation.

<!-- TODO:
## How are addresses compared, updated or added?
*
-->
