#include "cobalt/renderer/rasterizer/lib/imported/graphics.h"

// Empty implementations so that 'lib' targets can be compiled into executables
// by the builder without missing symbol definitions.
void CbLibOnGraphicsContextCreated() {}
void CbLibRenderFrame(intptr_t render_tree_texture_handle) {
  (void)render_tree_texture_handle;
}
