# Migration from `<permission>` to the MVP `<usermedia>` Capability Element

## Overview: The Evolution of PEPC
Following extensive real-world validation and collaborative discussions with the web standards community, the Page-Embedded Permission Controls ([PEPC](https://github.com/WICG/PEPC/blob/main/explainer.md)) initiative is evolving into a suite of specialized **Capability Elements**.

While the initial PEPC approach focused on managing permission states, these new **Capability Elements** function as direct controllers for web capabilities. Specialized elements such as `<geolocation>`, `<usermedia>`, `<camera>`, and `<microphone>` move beyond simply gating the functionality to directly providing functional utility to your applications.

For requesting camera and microphone access, the `<permission>` element is transitioning to the `<usermedia>` element. Targeting launch in M151, the `<usermedia>` element only supports combined camera and microphone access, with dedicated `<camera>` and `<microphone>` elements as fast follows in M152.

## Migration Path
The transition from the legacy `<permission type="camera microphone">` element (HTMLPermissionElement) to the `<usermedia>` Capability Element (HTMLUserMediaElement) is structured as a reverse origin trial. This approach preserves current behavior for existing users by setting a header (or meta tag) with the Origin Trial token, where `<usermedia>` continues to function as it has under the current origin trial.

The next phase, beginning with the general launch in M151, introduces the standard `<usermedia>` element as an active functional broker. In this phase (provided no legacy indicators like the Origin Trial token or flag are present), `<usermedia>` is an active controller that triggers stream acquisition immediately upon user grant. The element functions as a declarative counterpart to the `getUserMedia` API, and will activate hardware indicators, such as the camera light, which developers must appropriately handle—for instance, by stopping tracks if immediate access is not required.

---

## Legacy Support: UserMediaElement Origin Trial
The `<usermedia>` Capability Element is currently available via a dedicated Origin Trial. To activate this functionality, you must [register your origin](https://developer.chrome.com/origintrials/#/view_trial/3736298840857247745) and deploy the corresponding token. This trial period is temporary (scheduled to conclude at M152, though an extension to M155 is possible to provide more time for adoption).

## Step 1 & 2: Integration and Detection
Simply replace instances of the `<permission>` tag with `<usermedia>` in your markup.

Additionally, ensure that any CSS selectors specifically targeting the permission tag are updated to target `usermedia`. Similarly, any JavaScript queries using `document.getElementsByTagName("permission")` (or similar tag-based lookups) must be updated to reference the new tag name. This update should be applied across all related cases in your codebase.

**Before (Generic Permission Element):**

```css
/* CSS Selector */
permission { background: #eef; color: #333; border: 2px solid #007bff; }
```

```javascript
/* JavaScript Tag Lookup */
const legacyElement = document.getElementsByTagName("permission");

/* JavaScript Query Selector */
const btn = document.querySelector("permission");
```

```html
<permission type="camera microphone" id="media-request-btn">
  <button id="fallback-btn" onclick="requestMediaLegacy()">
    Enable Camera & Microphone
  </button>
</permission>
```

**After (New Capability Element):**

```html
<usermedia type="camera microphone" id="media-request-btn">
  <button id="fallback-btn" onclick="requestMediaLegacy()">
    Enable Camera & Microphone
  </button>
</usermedia>
```

```css
/* CSS Selector Update */
usermedia { background: #f0f0f0; border: 1px solid #ccc; }
usermedia:granted { color: green; }
```

```javascript
/* JavaScript Tag Lookup Update */
const newElement = document.getElementsByTagName("usermedia");

/* JavaScript Query Selector Update */
const btn = document.querySelector("usermedia");
```

## Step 2: Update JavaScript Feature Detection (If Applicable)
If your JavaScript explicitly checks for the `HTMLPermissionElement` interface to detect browser support, you simply need to update it to check for `HTMLUserMediaElement`.

**Before:**

```javascript
function browserSupportsPEPC() {
  return typeof HTMLPermissionElement === 'function';
}
```

**After:**

```javascript
function browserSupportsUserMediaElement() {
  return typeof HTMLUserMediaElement === 'function';
}
```

## Standard `<usermedia>` M151+
The full power of the `<usermedia>` Capability Element is unlocked from M151+. Once the element is enabled by default in M151 and the legacy Origin Trial token is removed from the site, it transitions from a simple permission gate to a media provider. Features such as the `.stream` property and the `.setConstraints()` method become functional only in this standard mode. Note that the MVP currently supports only combined camera and microphone access; dedicated `<camera>` and `<microphone>` elements remain future roadmap items and are not available in the initial launch.

```javascript
// Check if the new API is supported without creating a DOM element, the site then should remove the OT token to use the new API
if (typeof window.HTMLUserMediaElement !== 'undefined' &&
    typeof HTMLUserMediaElement.prototype.setConstraints === 'function') {
  console.log('Capability Element <usermedia> features are supported!');
} else {
  console.log('Falling back to legacy element handling.');
}
```

In standard `<usermedia>`, `.setConstraints()` allows you to define hardware preferences before user interaction (note that for security and predictability, constraints can typically be set once per media request cycle). Upon a successful grant, the `.stream` property provides a direct `MediaStream` reference. If your application logic relies on these properties during the current Origin Trial (legacy phase), please be aware they are currently no-ops. You should plan your migration to leverage these APIs once you transition away from the legacy indicators¹ following the general M151 release.

The `.stream` property is a read-only attribute in the MVP. It is important to note that once the stream is acquired, it starts immediately, matching the behavior of the `getUserMedia` API (for instance, triggering the camera's hardware light). If your site does not intend to use the stream right away, the application is responsible for manually stopping the tracks to release the hardware resources.

```javascript
const usermedia = document.getElementById('media-request-btn');

// Specify hardware preferences before interaction
usermedia.setConstraints({
  video: { width: 1280, height: 720},
  audio: { echoCancellation: true }
});

// Handle successful stream acquisition
usermedia.addEventListener("stream", () => {
    videoElement.srcObject = usermedia.stream;
});

// Handle stream acquisition failure
usermedia.addEventListener("error", () => {
    console.error(`Access failed: ${usermedia.error?.name}`);
});

// Handle prompt cancellation or dismissal
usermedia.addEventListener("cancel", () => {
    console.log("Permission prompt was dismissed by the user.");
});
```

---

## FAQ

**Will my existing CSS break?**

No. Any CSS classes or IDs applied to your previous `<permission>` elements will work flawlessly on `<usermedia>` elements, provided you update any direct tag-name CSS selectors (e.g., change `permission { ... }` to `usermedia { ... }`). The browser-enforced security rules for contrast and overlapping are identical.

**What is the status of events from the permission element like `promptaction`, and `promptdismiss`?**

In alignment with ongoing discussion within the web standards community, we are considering streamlining the API, which may involve the removal of these events. Our team will provide further updates regarding these decisions.

*Updated decision: these events are now only available when the element is operating in legacy mode¹.*

**What happens to users on older browser versions?**

Just like the previous iteration of PEPC, the `<usermedia>` element is designed for graceful deployment. Browsers that do not yet support `<usermedia>` will treat it as an `HTMLUnknownElement` and automatically render its child elements (your fallback buttons). Your legacy permission flows will continue to operate without interruption.

**Why are we making this change?**

After discussing closely with WebRTC group and other vendors, splitting all-in-one `<permission>` into specific Capability Elements (like `<usermedia>` and `<geolocation>`, and later with `<camera>` and `<microphone>`) allows browsers to offer more tailored user experiences that is not only controlling permission state but also the capability itself, clearer semantic HTML, and better modularity for future APIs.

**When will the type attribute be deprecated?**

The type only works in legacy mode, currently estimated at M152 milestones; however, this remains contingent upon the stabilization of the media constraints pipeline and the element’s adoption. Once these are stable, the type attribute will be entirely deprecated (if necessary, we can set an OT extension, potential M155 which would enable sites to defer the implementation keep the type attribute for an extended period).

**Does Origin Trial (OT) registration automatically enable the element, or is a separate feature flag required (e.g., in M148)?**

Registering for the Origin Trial and deploying the token is sufficient to enable the `<usermedia>` element on your registered origin. When the element is active via the OT, users visiting your site will not need to manually toggle a separate Chrome flag to access the feature. Upon the general release of `<usermedia>` in M151, the inclusion of an OT token specifies that the element will operate within its legacy mode¹.

---

*Notes:*

*¹ The element's legacy behavior is now determined exclusively by the presence of a valid Origin Trial token or the 'UserMediaElementLegacy' flag.*