import os
import glob

files_to_fix = [
    "media/starboard/decoder_buffer_allocator.cc",
    "media/starboard/starboard_renderer.cc",
    "net/http/http_cache_transaction.cc",
    "net/url_request/url_request.cc",
    "sql/database.cc",
    "sql/statement.cc",
    "third_party/blink/renderer/bindings/core/v8/v8_script_runner.cc",
    "third_party/blink/renderer/core/frame/local_frame_view.cc"
]

for file_path in files_to_fix:
    with open(file_path, 'r') as f:
        content = f.read()
    
    content = content.replace('cobalt::memory::ScopedMemoryContext', 'base::memory::ScopedMemoryContext')
    content = content.replace('cobalt::memory::MemoryContext', 'base::memory::MemoryContext')
    content = content.replace('#include "cobalt/memory/cobalt_memory_attribution_manager.h"', '#include "base/memory/cobalt_memory_context.h"')
    
    with open(file_path, 'w') as f:
        f.write(content)

print("Files fixed!")
