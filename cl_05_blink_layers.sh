#!/bin/bash
git restore --source=add-thread-based-memory-allocation-hooks \
  third_party/blink/renderer/core/BUILD.gn \
  third_party/blink/renderer/core/css/style_engine.cc \
  third_party/blink/renderer/core/dom/document.cc \
  third_party/blink/renderer/core/frame/local_frame_view.cc \
  third_party/blink/renderer/core/html/parser/html_document_parser.cc \
  third_party/blink/renderer/core/loader/resource/font_resource.cc \
  third_party/blink/renderer/core/paint/paint_layer_painter.cc \
  third_party/blink/renderer/platform/BUILD.gn \
  third_party/blink/renderer/platform/graphics/canvas_hibernation_handler.cc \
  third_party/blink/renderer/platform/graphics/image_frame_generator.cc \
  third_party/blink/renderer/platform/graphics/parkable_image.cc \
  third_party/blink/renderer/platform/loader/BUILD.gn \
  third_party/blink/renderer/platform/loader/fetch/resource_loader.cc \
  third_party/blink/renderer/modules/BUILD.gn \
  third_party/blink/renderer/modules/canvas/BUILD.gn \
  third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.cc \
  third_party/blink/renderer/modules/indexeddb/BUILD.gn \
  third_party/blink/renderer/modules/indexeddb/idb_transaction.cc \
  third_party/blink/renderer/modules/websockets/BUILD.gn \
  third_party/blink/renderer/modules/websockets/websocket_channel_impl.cc
