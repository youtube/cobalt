This directory contains exo-specific extensions to the Wayland protocol.

To begin with, we recommend this
[link](https://wayland-book.com/xdg-shell-basics/xdg-surface.html) for more
about wayland basics. The short summary is that:
* wl_surface is the compositing window primitive. It is capable of receiving a
  series of buffers representing contents. It only provides basic functionality.
  Other functionality like pip/decorations are implemented through extensions.
* It is possible to extend a wl_surface as xdg_surface via
  xdg_wm_base.get_surface. This extension is not permanent: it is possible to
  destroy and/or recreate the xdg_surface.
* Once an xdg_surface is created, it can be assigned a role: xdg_toplevel or
  xdg_popup.
* exo has extensions for each of these primitives that implement aura shell
  (ash) specific functionalities:
  * wl_surface is extended by zaura_surface
  * xdg_toplevel is extended by zaura_toplevel
  * xdg_popup is extended by zaura_popup

The wayland protocol is used to communicate between ash-chrome
(exo/wayland-server) and wayland clients. The lacros-chrome client is version
skewed from ash-chrome. As such, the protocol itself must be a stable API
surface. This has one main implication:
* It is not safe to remove any methods. This includes reverts of CLs that add
  methods.

This implication means we need to minimize risk of needing to revert CLs that
add methods. We thus add the following guidance:
* When adding a new interface method, create the exo (server) implementation
  first.
* In the same CL, add a stub (empty) implementation on the client
  (ozone-wayland) side without updating the version. This is to avoid a problem
  when yet another protocol update is added on the client side while your are
  working on the client side implementation for your protocol update.
* In a separate CL, follow up with the client changes that use the interface
  method.
Thus, in the event that usage of the new interface causes bugs, the client-side
change can be reverted without modifying the API surface itself.

Note that the following directories contain exo-specific extensions:
following directories contains exo-specific extensions
 * components/exo/wayland/protocol
 * third_party/wayland-protocols/unstable
