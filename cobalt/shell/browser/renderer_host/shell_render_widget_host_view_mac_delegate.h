// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_SHELL_BROWSER_RENDERER_HOST_SHELL_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_
#define COBALT_SHELL_BROWSER_RENDERER_HOST_SHELL_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_

#import <Cocoa/Cocoa.h>

#import "content/public/browser/render_widget_host_view_mac_delegate.h"

@interface ShellRenderWidgetHostViewMacDelegate
    : NSObject <RenderWidgetHostViewMacDelegate>

@end

#endif  // COBALT_SHELL_BROWSER_RENDERER_HOST_SHELL_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_
