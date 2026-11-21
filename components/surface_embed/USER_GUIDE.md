# Surface Embed User Guide

`//components/surface_embed` implements a mechanism to embed a
`content::WebContents` (the inner/guest page) inside another
`content::WebContents` (the outer/embedder page) by using a custom
`blink::WebPlugin`. It is utilized by **WebUI Browser** to embed tab contents
directly.

---

## 1. Setup in C++ (Browser Process)

### 1.1. Allow Your Frame to Use Surface-Embed
Currently, surface-embed is only enabled for `chrome://webui-browser`. If you
wish to allow another frame or WebUI domain to use surface-embed, you must:

1. Enable the feature flag [kSurfaceEmbed](https://source.chromium.org/chromium/chromium/src/+/main:components/surface_embed/common/features.h;l=17;drc=3d12ee18ff8660d03d307fcfc4d7cfca9c793e46).
2. Register the binder for `SurfaceEmbedHost` in [chrome_browser_interface_binders_webui_parts_desktop.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/chrome_browser_interface_binders_webui_parts_desktop.cc;l=521;drc=3d12ee18ff8660d03d307fcfc4d7cfca9c793e46):
   ```cpp
   #include "components/surface_embed/common/features.h"
   #include "components/surface_embed/browser/surface_embed_host.h"

   // ...
   if (base::FeatureList::IsEnabled(surface_embed::features::kSurfaceEmbed)) {
     map->Add<surface_embed::mojom::SurfaceEmbedHost>(base::BindRepeating(
         [](content::RenderFrameHost* render_frame_host,
            mojo::PendingReceiver<surface_embed::mojom::SurfaceEmbedHost> receiver) {
           auto* web_ui = render_frame_host->GetWebUI();
           // Security check: only allow surface-embed in WebUIBrowserUI.
           // Add your WebUIController here.
           if (!web_ui || !web_ui->GetController()->GetAs<WebUIBrowserUI>()) {
             return;
           }
           surface_embed::SurfaceEmbedHost::Create(render_frame_host, std::move(receiver));
         }));
   }
   ```
3. Update [ChromeContentRendererClient::OverrideCreatePlugin](https://source.chromium.org/chromium/chromium/src/+/main:chrome/renderer/chrome_content_renderer_client.cc;l=905-906;drc=a2f4b912ddca2b52a816da8603fe23fa5d1bf5a6)
to recognize your WebUI URL/host and allow the creation of the plugin:
   ```cpp
   if (url.host() == chrome::kChromeUIYourCustomHost) {
     if (surface_embed::MaybeCreatePlugin(render_frame, params, plugin)) {
       return true;
     }
   }
   ```

### 1.2. Configure Content Security Policy (CSP)
By default, WebUI blocks `<object>`s and `<embed>`s. You must explicitly
override the CSP `object-src` rule to allow `'self'` via [WebUIDataSource](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/web_ui_data_source.h;l=147;drc=d673b3a7277668f3e619493d8f91deca6616b07a).
```cpp
source->OverrideContentSecurityPolicy(
    network::mojom::CSPDirectiveName::ObjectSrc, "object-src 'self';");
```

### 1.3. How to Get the Content ID for a WebContents
To embed a guest `content::WebContents`, the frontend needs a unique identifier
corresponding to that guest. This is managed by [GuestContentsHandle](https://source.chromium.org/chromium/chromium/src/+/main:components/guest_contents/browser/guest_contents_handle.h;l=28;drc=d60f66038e8386eeee7708012e9b61c7ff8afb8d).

On the C++ side, associate a handle with the guest `WebContents` and retrieve
its assigned UUID (`GuestId` / `base::UnguessableToken`):
```cpp
#include "components/guest_contents/browser/guest_contents_handle.h"

// 1. Create or retrieve the handle for the target WebContents
guest_contents::GuestContentsHandle* guest_handle =
    guest_contents::GuestContentsHandle::CreateForWebContents(web_contents);

// 2. Get the unique ID token and pass its serialized string to the frontend
// (e.g. via Mojo message)
std::string content_id = guest_handle->id().ToString();
```

Surface Embed does not manage the lifecycle of `WebContents`. If the child
WebContents is destroyed, the `<embed>` will become blank. If the `<embed>` is
removed or set to a different `content-id`, the previously attached
`WebContents` will be detached.

---

## 2. Setup in HTML / JS (Frontend)

To embed the guest page on your WebUI frontend, simply instantiate the `<embed>`
tag with your guest's content ID.

### 2.1. Render the `<embed>` Element
In your template:

```typescript
import {html} from '//resources/lit/v3_0/lit.rollup.js';

// Inside your custom WebUI component render() helper:
return html`
  <embed class="content"
         type="application/x-chromium-surface-embed"
         data-content-id="${this.guestId}">
  </embed>
`;
```

### 2.2. Requirements for Attributes
* **`type`**: Must match [kInternalPluginMimeType](https://source.chromium.org/chromium/chromium/src/+/main:components/surface_embed/common/constants.h;l=11;drc=da966bf8542039f60d23ff8d922166ef30725b5d),
which is `"application/x-chromium-surface-embed"`.
* **`data-content-id`**: Must contain the serialized string representation of
the `guest_contents::GuestContentsHandle` token corresponding to the nested
`WebContents` (the `content_id` retrieved in Section 1.3).

When the `data-content-id` attribute changes, the custom plugin automatically
notices, parses the identifier, and communicates with the browser host to swap
the nested visual frame/surface instantly.
